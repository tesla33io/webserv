/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:10:22 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/19 15:57:55 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/Utils/ServerUtils.hpp"

/* Request handlers */

void WebServer::handleRequestTooLarge(Connection *conn, ssize_t bytes_read) {
	_lggr.info("Reached max content length for fd: " + su::to_string(conn->fd) + ", " +
	           su::to_string(bytes_read) + "/" +
	           su::to_string(conn->locConfig->getMaxBodySize()));
	prepareResponse(conn, Response(413, conn));
}

bool WebServer::handleCompleteRequest(Connection *conn) {
	processRequest(conn);

	_lggr.debug("Request was processed. Read buffer will be cleaned");
	conn->read_buffer.clear();
	conn->request_count++;
	conn->updateActivity();
	return true; // Continue processing
}

bool WebServer::handleCGIRequest(ClientRequest &req, Connection *conn) {
	Logger _lggr;

	CGI *cgi = CGIUtils::createCGI(req, conn->locConfig);
	if (!cgi)
		return (false);
	_cgi_pool[cgi->getOutputFd()] = std::make_pair(cgi, conn);
	if (!epollManage(EPOLL_CTL_ADD, cgi->getOutputFd(), EPOLLIN)) {
		_lggr.error("EPollManage for CGI request failed.");
		return (false);
	}
	return (true);
}

/* Request processing */

bool WebServer::isHeadersComplete(Connection *conn) {
	
	std::string temp = conn->read_buffer;
	size_t header_end = conn->read_buffer.find("\r\n\r\n");
	if (header_end == std::string::npos) {
		return false;
	}

	// Headers are complete
	std::string headers = conn->read_buffer.substr(0, header_end + 4);
	std::string headers_lower = su::to_lower(headers);
	
	// Header request for early headers error detection
	ClientRequest hdr_req;
	hdr_req.clfd = conn->fd;

	// On error: REQUEST_COMPLETE, Prepare Response
	uint16_t error_code = RequestParsingUtils::parseRequestHeaders(headers_lower, hdr_req);
	_lggr.debug("[HEADER CHECK] Parsing headers request: " + hdr_req.toMiniString());
	_lggr.debug("[HEADER CHECK] Error code post header request parsing : " + su::to_string(error_code));
	if (error_code != 0) {
		_lggr.error("Parsing of the request's headers failed.");
		conn->state = Connection::REQUEST_COMPLETE;
		_lggr.logWithPrefix(Logger::ERROR, "BAD REQUEST", "Malformed or invalid headers");
		prepareResponse(conn, Response(error_code, conn));
		return true;
	}

	// Store parsed headers in connection
	conn->headers_buffer = headers;
	conn->parsed_request = hdr_req;

	// Expect and no chunk -> to verify
	// if (expect_it != hdr_req.headers.end() && expect_it->second == "100-continue") {
	// 	// Send "100 Continue" response
	// 	prepareResponse(conn, Response::continue_());
	// 	conn->state = Connection::CONTINUE_SENT;
	// }

	// In HTTP/1.1, a message body can be delimited in exactly one of these ways:
	// Content-Length: N → body is exactly N bytes long.
	// Transfer-Encoding: chunked → body is streamed in chunks until a zero-length chunk.
	// No body expected → e.g. GET without body, or status codes like 204 / 304.
	// ! They are mutually exclusive !

	conn->chunked = hdr_req.chunked_encoding;
	conn->chunked = hdr_req.chunked_encoding;
	
	// Case 1 : content length is present
	if (hdr_req.content_length == 0) {
		conn->state = Connection::REQUEST_COMPLETE;
		return true;
	}
	
	// a GET request has no content length
	
	// Clear the read_buffer since we've processed headers and moved body to body_data
	conn->read_buffer.clear();
	conn->read_buffer = temp;

	conn->body_bytes_read = conn->body_data.size();
	conn->state = Connection::READING_BODY;

	if (static_cast<ssize_t>(conn->body_bytes_read) >= conn->content_length) {
		conn->state = Connection::REQUEST_COMPLETE;
		return true;
	}

	return false;

	} 
	else if (headers_lower.find("transfer-encoding: chunked") != std::string::npos) {
		conn->chunked = true;
		conn->headers_buffer = headers;

		if (headers_lower.find("expect: 100-continue") != std::string::npos) {
			prepareResponse(conn, Response::continue_());

			conn->state = Connection::CONTINUE_SENT;

			conn->read_buffer.clear();

			conn->chunk_size = 0;
			conn->chunk_bytes_read = 0;
			conn->chunk_data.clear();

			return true;
		} else {
			conn->state = Connection::READING_CHUNK_SIZE;

			conn->read_buffer = conn->read_buffer.substr(header_end + 4);

			conn->chunk_size = 0;
			conn->chunk_bytes_read = 0;
			conn->chunk_data.clear();

			return processChunkSize(conn);
		}
	} else {
		conn->chunked = false;
		conn->state = Connection::REQUEST_COMPLETE;
		return true;
	}
	return false;
}



bool WebServer::isRequestComplete(Connection *conn) {
	
	switch (conn->state) {
		
	case Connection::READING_HEADERS:
		_lggr.debug("isRequestComplete->READING_HEADERS");
		return isHeadersComplete(conn);

	case Connection::READING_BODY:
		_lggr.debug("isRequestComplete->READING_BODY");
		_lggr.debug(
		    su::to_string(conn->content_length - static_cast<ssize_t>(conn->body_data.size())) +
		    " bytes left to receive");
					
		if (static_cast<ssize_t>(conn->body_data.size()) >= conn->content_length) {
			_lggr.debug("Read full content-length: " + su::to_string(conn->body_data.size()) +
			            " bytes received");
			conn->state = Connection::REQUEST_COMPLETE;
			reconstructRequest(conn);
			return true;
		}
		return false;

		
	case Connection::CONTINUE_SENT:
		_lggr.debug("isRequestComplete->CONTINUE_SENT");
		conn->state = Connection::READING_CHUNK_SIZE;
		return processChunkSize(conn);

	case Connection::READING_CHUNK_SIZE:
		_lggr.debug("isRequestComplete->READING_CHUNK_SIZE");
		return processChunkSize(conn);

	case Connection::READING_CHUNK_DATA:
		_lggr.debug("isRequestComplete->READING_CHUNK_DATA");
		return processChunkData(conn);

	case Connection::READING_TRAILER:
		_lggr.debug("isRequestComplete->READING_TRAILER");
		return processTrailer(conn);

	case Connection::REQUEST_COMPLETE:
	case Connection::CHUNK_COMPLETE:
		_lggr.debug("isRequestComplete->REQUEST_COMPLETE");
		return true;

	default:
		_lggr.debug("isRequestComplete->default");
		return false;
	}
}


bool WebServer::reconstructRequest(Connection *conn) {
	std::string reconstructed_request;

	std::cout << "            INSIDE RECONSTRUCT\n";
	if (conn->headers_buffer.empty()) {
		_lggr.warn("Cannot reconstruct request: headers not available");
		return false;
	}

	std::cout << "                                     HEADER BUFFER: " << conn->headers_buffer << std::endl;
	reconstructed_request = conn->headers_buffer;
	std::cout << "                                     RECONSTR BUFFER: " << conn->headers_buffer << std::endl;

	if (conn->content_length > 0) {
		size_t body_size =
		    std::min(static_cast<size_t>(conn->content_length), conn->body_data.size());

		reconstructed_request.append(reinterpret_cast<const char *>(&conn->body_data[0]),
		                             body_size);

		_lggr.debug("Reconstructed request with " + su::to_string(body_size) +
		            " bytes of body data");
	}

	conn->read_buffer = reconstructed_request;

	size_t headers_end = conn->headers_buffer.size();
	std::string debug_output =
	    "Reconstructed request headers:\n" + conn->read_buffer.substr(0, headers_end);
	if (conn->content_length > 0) {
		debug_output += "\n[Binary body data: " + su::to_string(conn->body_data.size()) + " bytes]";
	}
	_lggr.debug(debug_output);

	return true;
}


bool WebServer::parseRequest(Connection *conn, ClientRequest &req) {
	_lggr.debug("Parsing request: " + req.toMiniString());
	uint16_t error_code = RequestParsingUtils::parseRequest(conn->read_buffer, req);
	_lggr.debug("Error code post request parsing : " + su::to_string(error_code));
	if (error_code != 0) {
		_lggr.error("Parsing of the request failed.");
		prepareResponse(conn, Response(error_code, conn));
		// closeConnection(conn);
		return false;
	}
	return true;
}

void WebServer::processRequest(Connection *conn) {
	_lggr.info("Processing request from fd: " + su::to_string(conn->fd));

	ClientRequest req;
	req.clfd = conn->fd;


	if (!parseRequest(conn, req))
		return;
	_lggr.debug("Request parsed successfully");

	_lggr.debug("req.path: " + req.path);
	_lggr.debug("req.uri: " + req.uri);

	// RFC 2068 Section 8.1 -- presistent connection unless client or server sets connection header
	// to 'close' -- indicating that the socket for this connection may be closed
	if (req.headers.find("connection") != req.headers.end()) {
		if (req.headers["connection"] == "close") {
			conn->keep_persistent_connection = false;
		}
	}

	if (req.chunked_encoding && conn->state == Connection::READING_HEADERS) {
		// Accept chunked requests sequence
		_lggr.debug("Accepting a chunked request");
		conn->state = Connection::READING_CHUNK_SIZE;
		conn->chunked = true;
		prepareResponse(conn, Response::continue_());
		return;
	}

	// TODO: this part breaks the req struct for some reason
	//       can't debug on my own :(
	if (req.chunked_encoding && conn->state == Connection::CHUNK_COMPLETE) {
		_lggr.debug("Chunked request completed!");
		_lggr.debug("Parsing complete chunked request");
		if (!parseRequest(conn, req))
			return;
		_lggr.debug("Chunked request parsed successfully");
		_lggr.debug(conn->toString());
		_lggr.debug(req.toString());
	}

	_lggr.debug("FD " + su::to_string(req.clfd) + " ClientRequest {" + req.toString() + "}");
	
	// Match location block, Normalize URI + Check traversal
	if (!matchLocation(req, conn) || !normalizePath(req, conn))	
		return;
	
	// process the request
	processValidRequest(req, conn);
}


bool WebServer::matchLocation(ClientRequest &req, Connection *conn) {
	// initialize the correct locConfig // default "/"
	LocConfig *match = findBestMatch(req.uri, conn->servConfig->getLocations());
	if (!match) {
		_lggr.error("[Resp] No matched location for : " + req.uri);
		prepareResponse(conn, Response::internalServerError(conn));
		return false;
	}
	conn->locConfig = match; 
	conn->locConfig->setFullPath("");
	_lggr.debug("[Resp] Matched location : " + conn->locConfig->path);
	return true;
}

bool WebServer::normalizePath(ClientRequest &req, Connection *conn) {

	// normalisation
	std::string full_path = buildFullPath(req.path, conn->locConfig);
	std::string root_full_path = buildFullPath("", conn->locConfig);
	char resolved[PATH_MAX];
	realpath(full_path.c_str(), resolved);
	std::string normal_full_path(resolved);
	if (su::back(full_path) == '/')
		normal_full_path += "/";
	_lggr.debug("[Resp] Normalized full path : " + normal_full_path);
	_lggr.debug("[Resp] Root full path : " + root_full_path);

	// std::string temp_full_path = normal_full_path + "/";
	if (normal_full_path.compare(0, root_full_path.size(), root_full_path) != 0) {
		_lggr.error("Resolved path is trying to access parent directory: " + normal_full_path);
		prepareResponse(conn, Response::forbidden(conn));
		return false;
	}
	
	// this should maybe be in the connection info, not in the locConfig
	conn->locConfig->setFullPath(normal_full_path);
	return true;
}


void WebServer::processValidRequest(ClientRequest &req, Connection *conn) {
		
	const std::string& full_path = conn->locConfig->getFullPath();
	_lggr.debug("[Resp] The matched location is an exact match: " + su::to_string(conn->locConfig->is_exact_()));

	// Check against location's max body size
	_lggr.logWithPrefix(Logger::DEBUG, "HTTP", 
		                 "Request body is: " + su::to_string(req.body.size()) + 
			                 " bytes, limit in the block  is " + su::to_string(conn->locConfig->getMaxBodySize()));
	if (!conn->locConfig->infiniteBodySize() && 
	    static_cast<size_t>(req.body.size()) > conn->locConfig->getMaxBodySize()) {
		_lggr.logWithPrefix(Logger::WARNING, "HTTP", 
		                 "Request body too large: " + su::to_string(req.body.size()) + 
			                 " bytes exceeds limit of " + su::to_string(conn->locConfig->getMaxBodySize()));
		prepareResponse(conn, Response::contentTooLarge(conn));
		return;
	}
	
	// check if RETURN directive in the matched location
	if (conn->locConfig->hasReturn() && conn->locConfig->is_exact_()) {
		_lggr.debug("[Resp] The matched location has a return directive.");
		uint16_t code = conn->locConfig->return_code;
		std::string target = conn->locConfig->return_target;
		prepareResponse(conn, respReturnDirective(conn, code, target));
		return;
	}
	
	// method allowed?
	if (!conn->locConfig->hasMethod(req.method)) {
		_lggr.warn("[Resp] Method " + req.method + " is not allowed for location " +
		          conn->locConfig->path);
		prepareResponse(conn, Response::methodNotAllowed(conn, conn->locConfig->getAllowedMethodsString()));
		return;
	}
	
	// File system check 
	FileType file_type = checkFileType(full_path);

	// File system errors
	if (!handleFileSystemErrors(file_type, full_path, conn))
		return;
		
	bool end_slash = (!req.uri.empty() && su::back(req.uri) == '/');

	// Route based on file type and request format
	if (file_type == ISDIR) {
		handleDirectoryRequest(req, conn, end_slash);
	} else if (file_type == ISREG) {
		handleFileRequest(req, conn, end_slash);
	} else {
		_lggr.error("Unexpected file type for: " + full_path);
		prepareResponse(conn, Response::notFound(conn));
	}
}

