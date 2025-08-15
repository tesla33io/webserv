/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:10:22 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/15 10:57:58 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"

/* Request handlers */

void WebServer::handleRequestTooLarge(Connection *conn, ssize_t bytes_read) {
	_lggr.info("Reached max content length for fd: " + su::to_string(conn->fd) + ", " +
	           su::to_string(bytes_read) + "/" +
	           su::to_string(conn->getServerConfig()->getMaxBodySize()));
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
	size_t header_end = conn->read_buffer.find("\r\n\r\n");
	if (header_end == std::string::npos) {
		return false;
	}

	// Headers are complete, check if this is a chunked request
	std::string headers = conn->read_buffer.substr(0, header_end + 4);
	std::string headers_lower = su::to_lower(headers);

	if (headers_lower.find("content-length: ") != std::string::npos) {
		_lggr.debug("Found `Content-Length` header");
		size_t cl_start = headers_lower.find("content-length: ") + 16;
		size_t cl_end = headers_lower.find("\r\n", cl_start);

		if (cl_end == std::string::npos) {
			// TODO: handle malformed header
			_lggr.error("Malformed header");
			conn->content_length = -1;
			conn->state = Connection::REQUEST_COMPLETE;
			_lggr.logWithPrefix(Logger::ERROR, "BAD REQUEST", "Malformed headers");
			prepareResponse(conn, Response::badRequest());
			return true;
		}

		std::string cl_value = headers.substr(cl_start, cl_end - cl_start);

		char *endptr;
		long parsed_length = std::strtol(cl_value.c_str(), &endptr, 10);

		if (*endptr != '\0' || parsed_length < 0) {
			conn->content_length = -1;
		} else {
			conn->content_length = static_cast<ssize_t>(parsed_length);
		}

		conn->chunked = false;
		conn->headers_buffer = headers;

		// Handle any body data that came with headers
		std::string remaining_data = conn->read_buffer.substr(header_end + 4);
		if (!remaining_data.empty()) {
			// Store remaining data as binary body data
			conn->body_data.insert(conn->body_data.end(),
			                       reinterpret_cast<const unsigned char *>(remaining_data.data()),
			                       reinterpret_cast<const unsigned char *>(remaining_data.data() +
			                                                               remaining_data.size()));
		}

		// Clear the read_buffer since we've processed headers and moved body to body_data
		conn->read_buffer.clear();

		conn->body_bytes_read = conn->body_data.size();
		conn->state = Connection::READING_BODY;

		if (static_cast<ssize_t>(conn->body_bytes_read) >= conn->content_length) {
			conn->state = Connection::REQUEST_COMPLETE;
			return true;
		}

		return false;

	} else if (headers_lower.find("transfer-encoding: chunked") != std::string::npos) {
		conn->chunked = true;
		// conn->state = Connection::READING_CHUNK_SIZE;
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

bool WebServer::parseRequest(Connection *conn, ClientRequest &req) {
	_lggr.debug("Parsing request: " + conn->toString());
	if (!RequestParsingUtils::parseRequest(conn->read_buffer, req)) {
		_lggr.error("Parsing of the request failed.");
		_lggr.debug("FD " + su::to_string(conn->fd) + " " + conn->toString());
		prepareResponse(conn, Response(g_error_status, conn));
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
	if (!setupRequestContext(req, conn))
		return;
	
	// process the request
	processValidRequest(req, conn);
}


bool WebServer::setupRequestContext(ClientRequest &req, Connection *conn) {

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


void WebServer::handleDirectoryRequest(ClientRequest &req, Connection *conn, bool end_slash) {

	const std::string full_path =  conn->locConfig->getFullPath();
	
	_lggr.debug("Directory request: " + full_path);
	if (!end_slash ) {  //&& !conn->locConfig->is_exact_()
		_lggr.debug("Directory request without trailing slash, redirecting: " + req.uri);
		std::string redirectPath = req.uri + "/";
		prepareResponse(conn, respReturnDirective(conn, 301, redirectPath));
		return;
	} else {
		prepareResponse(conn, respDirectoryRequest(conn, full_path));
		return;
	}
}

void  WebServer::handleFileRequest(ClientRequest &req, Connection *conn, bool end_slash) {

	const std::string full_path =  conn->locConfig->getFullPath();
	_lggr.debug("File request: " + full_path);
	
	// Trailing '/'? Redirect
	if (end_slash ) { //&& !conn->locConfig->is_exact_()
		_lggr.debug("File request with trailing slash, redirecting: " + req.uri);
		std::string redirectPath = req.uri.substr(0, req.uri.length() - 1);
		prepareResponse(conn, respReturnDirective(conn, 301, redirectPath));
		return;
	}

	// HANDLE CGI
	std::string extension = getExtension(full_path);
	if (conn->locConfig->acceptExtension(extension)) {
		std::string interpreter = conn->locConfig->getExtensionPath(extension);
		_lggr.debug("CGI request, interpreter location : " + interpreter);
		req.extension = extension;
		if (!handleCGIRequest(req, conn)) {
			_lggr.error("Handling the CGI request failed.");
			prepareResponse(conn, Response::internalServerError(conn));
		}
		return;
	}

	// HANDLE STATIC GET RESPONSE
	if (req.method == "GET") {
		_lggr.debug("Static file GET request");
		prepareResponse(conn, respFileRequest(conn, full_path));
		return;
	} else {
		_lggr.debug("Non-GET request for static file - not implemented");
		prepareResponse(conn, Response::notImplemented(conn)); 
		return;
	}
}


bool WebServer::handleFileSystemErrors(FileType file_type, const std::string& full_path, Connection *conn) {
	if (file_type == NOT_FOUND_404) {
		_lggr.debug("[Resp] Could not open : " + full_path);
		prepareResponse(conn, Response::notFound(conn));
		return false;
	}
	if (file_type == PERMISSION_DENIED_403) {
		_lggr.debug("[Resp] Permission denied : " + full_path);
		prepareResponse(conn, Response::forbidden(conn));
		return false;
	}
	if (file_type == FILE_SYSTEM_ERROR_500) {
		_lggr.debug("[Resp] Other file access problem : " + full_path);
		prepareResponse(conn, Response::internalServerError(conn));
		return false;
	}
	return true;
}

bool WebServer::reconstructRequest(Connection *conn) {
	std::string reconstructed_request;

	if (conn->headers_buffer.empty()) {
		_lggr.warn("Cannot reconstruct request: headers not available");
		return false;
	}

	reconstructed_request = conn->headers_buffer;

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

