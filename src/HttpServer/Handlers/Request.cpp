/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:10:22 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/25 14:58:59 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/HttpServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/Utils/ServerUtils.hpp"

/* Request handlers */ // deprecated
void WebServer::handleRequestTooLarge(Connection *conn, ssize_t bytes_read) {
	_lggr.info("Reached max content length for fd: " + su::to_string(conn->fd) + ", " +
	           su::to_string(bytes_read) + "/" + su::to_string(conn->locConfig->getMaxBodySize()));
	prepareResponse(conn, Response(413, conn));
}

bool WebServer::handleCompleteRequest(Connection *conn) {
	processRequest(conn);

	_lggr.debug("Request was processed. Read buffer will be cleaned");
	conn->read_buffer.clear();
	conn->request_count++;
	conn->updateActivity();
	return true; 
}

uint16_t WebServer::handleCGIRequest(ClientRequest &req, Connection *conn) {
	Logger _lggr;

	CGI *cgi = NULL;
	uint16_t exit_code = CGIUtils::createCGI(cgi, req, conn->locConfig);
	if (exit_code)
		return (exit_code);

	_cgi_pool[cgi->getOutputFd()] = std::make_pair(cgi, conn);
	if (!epollManage(EPOLL_CTL_ADD, cgi->getOutputFd(), EPOLLIN)) {
		_lggr.error("EPollManage for CGI request failed.");
		return (502);
	}

	return (0);
}

/* Request processing */

bool WebServer::isHeadersComplete(Connection *conn) {
	_lggr.debug("isHeadersComplete");
	std::string temp = conn->read_buffer;
	size_t header_end = conn->read_buffer.find("\r\n\r\n");
	if (header_end == std::string::npos) {
		_lggr.debug("[HEADER CHECK] INCOMPLETE returning false");
		return false;
	}

	// Headers are complete
	std::string headers = conn->read_buffer.substr(0, header_end + 4);
	std::string remaining_data = conn->read_buffer.substr(header_end + 4);

	// Header request for early headers error detection
	ClientRequest req;
	req.clfd = conn->fd;

	// On error: REQUEST_COMPLETE, Prepare Response
	uint16_t error_code = RequestParsingUtils::parseRequestHeaders(headers, req, _lggr);
	_lggr.debug("[HEADER CHECK] Status post header request parsing : " + su::to_string(error_code));
	if (error_code != 0) {
		_lggr.logWithPrefix(Logger::ERROR, "BAD REQUEST", "Malformed or invalid headers");
		prepareResponse(conn, Response(error_code, conn));
		conn->state = Connection::REQUEST_COMPLETE;
		conn->should_close = true;
		return true;
	}

	// Match location block, Normalize URI + Check traversal
	if (!matchLocation(req, conn) || !normalizePath(req, conn)) {
		conn->state = Connection::REQUEST_COMPLETE;
		conn->should_close = true;
		return true;
	}
	// Return, Method, Max body
	if (!processValidRequestChecks(req, conn)) {
		conn->state = Connection::REQUEST_COMPLETE;
		conn->should_close = true;
		return true;
	}
	// Valid request headers - store parsed headers in connection
	conn->headers_buffer = headers;
	conn->parsed_request = req;
	conn->chunked = req.chunked_encoding;
	conn->content_length = req.content_length;

	if (!conn->chunked) { 	//Store remaining data as binary body data for Content-Length requests

		if (!remaining_data.empty() && conn->content_length > 0) {
			conn->body_data.insert(conn->body_data.end(),
								reinterpret_cast<const unsigned char *>(remaining_data.data()),
								reinterpret_cast<const unsigned char *>(remaining_data.data() + remaining_data.size()));
			conn->body_bytes_read = conn->body_data.size();
		}
		_lggr.debug("Request POST HEADER content length: " + su::to_string(conn->content_length));
		_lggr.debug("Request POST HEADER remaining data size: " + su::to_string(remaining_data.size()));

		// ERROR handling if Body present when it should not
		if (conn->content_length <= 0 && conn->body_bytes_read != 0) {
			_lggr.debug("Body present when it should not: send 400");
			prepareResponse(conn, Response(400, conn));
			conn->should_close = true;
			conn->state = Connection::REQUEST_COMPLETE;
			return true;
		}
		
		// Case : content_length 0 or no content length)
		if (conn->content_length <= 0) {
			conn->state = Connection::REQUEST_COMPLETE;
			return true;
		}
		// Case : content_length specified
		else { // if (conn->content_length > 0)
			conn->state = Connection::READING_BODY;
			// check if full body
			if (static_cast<ssize_t>(conn->body_data.size()) == conn->content_length) {
				conn->state = Connection::REQUEST_COMPLETE;	
				// req.body = reconstructRequest(conn); 
				_lggr.debug("1 req.body" + req.body);
				return true;
			}
			if (static_cast<ssize_t>(conn->body_data.size()) > conn->content_length) {
				conn->state = Connection::REQUEST_COMPLETE;	
				prepareResponse(conn, Response(400));
				_lggr.debug("Content length mismatch" + req.body);
				conn->should_close = true;
				return true;
			}
			// clear read_buffer since body data is in body_data vector
			conn->read_buffer.clear();
			return false;
		}
	}
	
	else { // CHUNKED - store remaining data as string 
		conn->state = Connection::READING_CHUNK_SIZE;
		conn->read_buffer = remaining_data; // Keep any data after headers for chunk processing
		conn->chunk_size = 0;
		conn->chunk_bytes_read = 0;
		conn->chunk_data.clear();
		return processChunkSize(conn);
	}
	// Default
	_lggr.logWithPrefix(Logger::ERROR, "BAD REQUEST", "Impossible request");
	conn->state = Connection::REQUEST_COMPLETE;
	return true;
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
					
		if (static_cast<ssize_t>(conn->body_data.size()) == conn->content_length) {
			_lggr.debug("Read full content-length: " + su::to_string(conn->body_data.size()) +
			            " bytes received");
			conn->state = Connection::REQUEST_COMPLETE;
			reconstructRequest(conn);
			return true;
		}
		return false;

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

	return true;
}

// Deprecated
bool WebServer::parseRequest(Connection *conn, ClientRequest &req) {
	_lggr.debug("Parsing request: " + conn->read_buffer);
	uint16_t error_code = RequestParsingUtils::parseRequest(conn->read_buffer, req, _lggr);
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

	ClientRequest req = conn->parsed_request;
	
	 // Handle body extraction differently for chunked vs non-chunked
	if (req.chunked_encoding) {
		// For chunked requests, use the reconstructed chunk data
		req.body = conn->chunk_data;
		_lggr.debug("Using chunked body data: " + su::to_string(req.body.length()) + " bytes");
	} else if (!req.chunked_encoding && conn->headers_buffer.size() <= conn->read_buffer.size()) {
		req.body = conn->read_buffer.substr(conn->headers_buffer.size());
	} else {
		_lggr.debug("No body data or headers not properly parsed");
		req.body = "";
	}
	
	_lggr.debug("req.body: " + req.body);
	_lggr.debug("req.headers: " + conn->headers_buffer);
	_lggr.debug("req.uri: " + req.uri);

	// For chunked requests: use of chunk_data length for content verification
	size_t actual_body_size = req.chunked_encoding ? conn->chunk_data.size() : req.body.size();
	
	_lggr.debug("[Resp] Payload vs content size: " + su::to_string(req.content_length) 
		            + ", payload size: " + su::to_string(actual_body_size) );
	
	// Only verify content-length for non-chunked requests
	if (!req.chunked_encoding && req.content_length >= 0 && static_cast<ssize_t>(actual_body_size) != req.content_length) {
		_lggr.error("[Resp] Payload mismatch, content size: " + su::to_string(req.content_length) 
		            + ", payload size: " + su::to_string(actual_body_size) );
		prepareResponse(conn, Response::contentTooLarge(conn));
		return;
	}

	_lggr.debug("FD " + su::to_string(req.clfd) + " ClientRequest {" + req.toString() + "}");
	// process the request
	processValidRequest(req, conn);
}


void WebServer::processValidRequest(ClientRequest &req, Connection *conn) {
		
	const std::string& full_path = conn->locConfig->getFullPath();
	_lggr.debug("[Resp] The matched location is an exact match: " + su::to_string(conn->locConfig->is_exact_()));
	
	// File system check 
	FileType file_type = checkFileType(full_path);
	_lggr.debug("[Resp] checkFileType for " + full_path + " is " + fileTypeToString(file_type));

	if (file_type == NOT_FOUND_404 && su::back(full_path) == '/') {
		std::string pathWithoutSlash = full_path.substr(0, full_path.length() - 1);
		FileType fileTypeWithoutSlash = checkFileType(pathWithoutSlash);
		if (fileTypeWithoutSlash == ISREG) {
			file_type = ISREG;
		}
	}
	
	// File system errors
	if (!handleFileSystemErrors(file_type, full_path, conn))
		return;

	// we redirect if uri is missing the / (and vice versa), not the resolved path
	bool end_slash = (!req.uri.empty() && su::back(req.uri) == '/');
	// Route based on file type and request format
	if (file_type == ISDIR) {
		handleDirectoryRequest(req, conn, end_slash);
	} else if (file_type == ISREG) {
		handleFileRequest(req, conn, end_slash);
	} else {
		_lggr.error("Unexpected file type for: " + full_path);
		prepareResponse(conn, Response::internalServerError(conn));
	}
}
