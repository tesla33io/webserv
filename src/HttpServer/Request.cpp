/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:10:22 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 18:13:46 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpServer.hpp"

void WebServer::handleRequestTooLarge(Connection *conn, ssize_t bytes_read) {
	_lggr.info("Reached max content length for fd: " + su::to_string(conn->fd) + ", " +
	           su::to_string(bytes_read) + "/" +
	           su::to_string(conn->getServerConfig()->getMaxBodySize()));
	prepareResponse(conn, Response(413, conn));
	// closeConnection(conn);
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

void WebServer::processRequest(Connection *conn) {
	_lggr.info("Processing request from fd: " + su::to_string(conn->fd));

	ClientRequest req;
	req.clfd = conn->fd;

	if (!parseRequest(conn, req))
		return;
	_lggr.debug("Request parsed successfully");

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
	std::string response;
	// initialize the correct locConfig // default "/"
	LocConfig *match = findBestMatch(req.uri, conn->servConfig->locations);
	if (!match) {
		_lggr.error("[Resp] No matched location for : " + req.uri);
		prepareResponse(conn, Response::internalServerError(conn));
		return;
	}
	conn->locConfig = match; // Set location context
	_lggr.debug("[Resp->getRoot()] Matched location : " + match->path);

	
	std::string full_path = buildFullPath(req.path, conn->locConfig);
	
	// parent directory (for the full path, removing filename if present)
	std::string parent_dir;
	if (full_path.find('/') != std::string::npos)
		parent_dir = full_path.substr(0, full_path.find_last_of('/'));
	else 
		parent_dir = ".";
	
	// security check- resolve the full path, not just parent
	char resolved[PATH_MAX];
	if (realpath(parent_dir.c_str(), resolved) == NULL) {
		_lggr.error("Could not resolve parent directory: " + full_path);
		prepareResponse(conn, Response::forbidden(conn));
		return;
	}
	std::string resolved_str(resolved);
	std::string prefix = (_root_prefix_path[_root_prefix_path.length() - 1] == '/')
				? _root_prefix_path.substr(0, _root_prefix_path.length() - 1)
				: _root_prefix_path;
	std::string full_root = prefix + match->root;
	
	
	if (!conn->locConfig->root.empty() 
	&& resolved_str.substr(0, full_root.length()) != full_root) {
		_lggr.error("Resolved path trying to access parent directory: " + resolved_str);
		prepareResponse(conn, Response::forbidden(conn));
		return;
	}
	
	
		// uri request ends with '/'
		bool end_slash = (!req.uri.empty() && req.uri[req.uri.length() - 1] == '/');
	
	// this should maybe be in the connection info, not in the locConfig
	// or at least should be cleared when the request is processed
	conn->locConfig->setFullPath(full_path);

	
	FileType ftype = checkFileType(full_path);
	
	// check if RETURN directive in the matched location
	if (conn->locConfig->hasReturn()) {
		_lggr.debug("[Resp] The matched location has a return directive.");
		uint16_t code = conn->locConfig->return_code;
		std::string target = conn->locConfig->return_target;
		prepareResponse(conn, handleReturnDirective(conn, code, target));
		return;
	}

	// Is the method allowed?
	if (!conn->locConfig->hasMethod(req.method)) {
		_lggr.warn("Method " + req.method + " is not allowed for location " +
		           conn->locConfig->path);
		prepareResponse(conn, Response::methodNotAllowed(conn));
		// TODO: the response must include the methods allowed
		return;
	}


	// Error checking
	if (ftype == NOT_FOUND_404) {
		_lggr.debug("Could not open : " + full_path);
		prepareResponse(conn, Response::notFound(conn));
		return;
	}
	if (ftype == PERMISSION_DENIED_403) {
		_lggr.debug("Permission denied : " + full_path);
		prepareResponse(conn, Response::forbidden(conn));
		return;
	}
	if (ftype == FILE_SYSTEM_ERROR_500) {
		_lggr.debug("Other file access problem : " + full_path);
		prepareResponse(conn, Response::internalServerError(conn));
		return;
	}


	// Directory requests
	if (ftype == ISDIR) {
		_lggr.debug("Directory request: " + full_path);
		if (!end_slash) {
			_lggr.debug("Directory request without trailing slash, redirecting: " + req.uri);
			std::string redirectPath = req.uri + "/";
			prepareResponse(conn, handleReturnDirective(conn, 301, redirectPath));
			return;
		} else {
			prepareResponse(conn, handleDirectoryRequest(conn, full_path));
			return;
		}
	}

	// Handle file requests with trailing /
	_lggr.debug("File request: " + full_path);
	if (ftype == ISREG && end_slash) {
		_lggr.debug("File request with trailing slash, redirecting: " + req.uri);
		std::string redirectPath = req.uri.substr(0, req.uri.length() - 1);
		prepareResponse(conn, handleReturnDirective(conn, 301, redirectPath));
		return;
	}

	// if we arrive here, this should be the only possible case
	if (ftype == ISREG && !end_slash) {
		_lggr.debug("File request with following extension: " + getExtension(req.uri));

		// check if it is a script with a language supported by the location
		if (conn->locConfig->acceptExtension(getExtension(req.path))) {
			std::string extPath = conn->locConfig->getExtensionPath(getExtension(req.path));
			_lggr.debug("Extension path is : " + extPath);
			req.extension = getExtension(req.path);
			if (!handleCGIRequest(req, conn)) {
				_lggr.error("Handling the CGI request failed.");
				prepareResponse(conn, Response::badRequest());
				return;
			}
			return;
		}

		if (req.method == "GET") {
			_lggr.debug("GET request not handled by CGI.");
			prepareResponse(conn, handleFileRequest(conn, full_path));
			return;
		} else {
			_lggr.debug("POST or DELETE request not handled by CGI -> not implemented response.");
			prepareResponse(conn, Response::notImplemented(conn)); 
			return;
		}
	}

	// if we arrive here, this should be the only possible case
	_lggr.debug("Should never be reached");
	prepareResponse(conn, Response::internalServerError(conn));
	return;
}


bool WebServer::parseRequest(Connection *conn, ClientRequest &req) {
	_lggr.debug("Parsing request: " + conn->toString());
	if (!RequestParsingUtils::parseRequest(conn->read_buffer, req)) {
		_lggr.error("Parsing of the request failed.");
		_lggr.debug("FD " + su::to_string(conn->fd) + " " + conn->toString());
		prepareResponse(conn, Response::badRequest(conn));
		// closeConnection(conn);
		return false;
	}
	return true;
}

bool WebServer::isRequestComplete(Connection *conn) {
	switch (conn->state) {
	case Connection::READING_HEADERS:
		_lggr.debug("isRequestComplete->READING_HEADERS");
		return isHeadersComplete(conn);

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

bool WebServer::isHeadersComplete(Connection *conn) {
	size_t header_end = conn->read_buffer.find("\r\n\r\n");
	if (header_end == std::string::npos) {
		return false;
	}

	// Headers are complete, check if this is a chunked request
	std::string headers = conn->read_buffer.substr(0, header_end + 4);

	std::string headers_lower = su::to_lower(headers);

	if (headers_lower.find("transfer-encoding: chunked") != std::string::npos) {
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
}

bool WebServer::processChunkSize(Connection *conn) {
	size_t crlf_pos = findCRLF(conn->read_buffer);
	if (crlf_pos == std::string::npos) {
		// Need more data to read chunk size
		return false;
	}

	std::string chunk_size_line = conn->read_buffer.substr(0, crlf_pos);

	conn->read_buffer = conn->read_buffer.substr(crlf_pos + 2);

	// ignore chunk extensions after ';'
	size_t semicolon_pos = chunk_size_line.find(';');
	if (semicolon_pos != std::string::npos) {
		chunk_size_line = chunk_size_line.substr(0, semicolon_pos);
	}

	chunk_size_line = su::trim(chunk_size_line);

	// TODO: Check for negative?
	conn->chunk_size = static_cast<size_t>(std::strtol(chunk_size_line.c_str(), NULL, 16));
	conn->chunk_bytes_read = 0;

	_lggr.debug("Chunk size: " + su::to_string(conn->chunk_size));

	if (conn->chunk_size == 0) {
		// Last chunk, read trailers
		conn->state = Connection::READING_TRAILER;
		return processTrailer(conn);
	} else {
		conn->state = Connection::READING_CHUNK_DATA;
		return processChunkData(conn);
	}
}

bool WebServer::processChunkData(Connection *conn) {
	size_t available_data = conn->read_buffer.length();
	size_t bytes_needed = conn->chunk_size - conn->chunk_bytes_read;

	if (available_data < bytes_needed + 2) { // +2 for trailing CRLF
		// Need more data
		return false;
	}

	size_t bytes_to_read = bytes_needed;
	std::string chunk_part = conn->read_buffer.substr(0, bytes_to_read);
	conn->chunk_data += chunk_part;
	conn->chunk_bytes_read += bytes_to_read;

	conn->read_buffer = conn->read_buffer.substr(bytes_to_read);

	// Check if there are trailing CRLF
	if (conn->read_buffer.length() < 2) {
		return false;
	}

	if (conn->read_buffer.substr(0, 2) != "\r\n") {
		_lggr.error("Invalid chunk format: missing trailing CRLF");
		return false;
	}

	// Remove trailing CRLF
	conn->read_buffer = conn->read_buffer.substr(2);

	conn->state = Connection::READING_CHUNK_SIZE;
	return processChunkSize(conn);
}

bool WebServer::processTrailer(Connection *conn) {
	size_t trailer_end = findCRLF(conn->read_buffer);

	if (trailer_end == std::string::npos) {
		// Need more data
		return false;
	}

	std::string trailer_line = conn->read_buffer.substr(0, trailer_end);
	conn->read_buffer = conn->read_buffer.substr(trailer_end + 2);

	// If trailer line is empty, we're done
	if (trailer_line.empty()) {
		conn->state = Connection::CHUNK_COMPLETE;

		reconstructChunkedRequest(conn);
		return true;
	}

	return processTrailer(conn);
}

// TODO: add more debuggin info
void WebServer::reconstructChunkedRequest(Connection *conn) {
	std::string reconstructed_request = conn->headers_buffer;

	std::string headers_lower = su::to_lower(reconstructed_request);

	size_t te_pos = headers_lower.find("transfer-encoding: chunked");
	if (te_pos != std::string::npos) {
		// Find the end of this header line
		size_t line_end = reconstructed_request.find("\r\n", te_pos);
		if (line_end != std::string::npos) {
			// Remove the Transfer-Encoding line
			reconstructed_request.erase(te_pos, line_end - te_pos + 2);
		}
	}

	// Add Content-Length header before the final CRLF
	size_t final_crlf = reconstructed_request.find("\r\n\r\n");
	if (final_crlf != std::string::npos) {
		std::string content_length_header =
		    "\r\nContent-Length: " + su::to_string(conn->chunk_data.length()) + "\r\n";
		reconstructed_request.insert(final_crlf, content_length_header);
	}

	conn->read_buffer = reconstructed_request + conn->chunk_data;
	conn->state = Connection::REQUEST_COMPLETE;

	_lggr.debug("Reconstructed chunked request, total body size: " +
	            su::to_string(conn->chunk_data.length()));
}
