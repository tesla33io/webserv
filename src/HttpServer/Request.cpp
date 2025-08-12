#include "HttpServer.hpp"
#include "src/Logger/Logger.hpp"
#include "src/RequestParser/request_parser.hpp"
#include "src/Utils/StringUtils.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <netdb.h>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>

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
	_lggr.debug("Request parsed sucsessfully");

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
		_lggr.debug("Chunked request parsed sucsessfully");
		_lggr.debug(conn->toString());
		_lggr.debug(req.toString());
	}

	//_lggr.debug("FD " + su::to_string(req.clfd) + " ClientRequest {" + req.toString() + "}");
	std::string response;
	// initialize the correct locConfig // default "/"
	LocConfig *match = findBestMatch(req.uri, conn->servConfig->locations);
	if (!match) {
		_lggr.error("[Resp] No matched location for : " + req.uri);
		prepareResponse(conn, Response::internalServerError(conn));
		return;
	}
	conn->locConfig = match; // Set location context
	_lggr.debug("[Resp] Matched location : " + match->path);

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
		return;
	}

	std::string fullPath = buildFullPath(req.path, conn->locConfig);
	conn->locConfig->setFullPath(fullPath);
	// security check
	// TODO : normalize the path
	if (fullPath.find("..") != std::string::npos) {
		_lggr.warn("Uri " + req.uri + " is not safe.");
		prepareResponse(conn, Response::forbidden(conn));
		return;
	}

	FileType ftype = checkFileType(fullPath);

	// Error checking
	if (ftype == NOT_FOUND_404) {
		_lggr.debug("Could not open : " + fullPath);
		prepareResponse(conn, Response::notFound(conn));
		return;
	}
	if (ftype == PERMISSION_DENIED_403) {
		_lggr.debug("Permission denied : " + fullPath);
		prepareResponse(conn, Response::forbidden(conn));
		return;
	}
	if (ftype == FILE_SYSTEM_ERROR_500) {
		_lggr.debug("Other file access problem : " + fullPath);
		prepareResponse(conn, Response::internalServerError(conn));
		return;
	}

	// uri request ends with '/'
	bool endSlash = (!req.uri.empty() && req.uri[req.uri.length() - 1] == '/');

	// Directory requests
	if (ftype == ISDIR) {
		_lggr.debug("Directory request: " + fullPath);
		if (!endSlash) {
			_lggr.debug("Directory request without trailing slash, redirecting: " + req.uri);
			std::string redirectPath = req.uri + "/";
			prepareResponse(conn, handleReturnDirective(conn, 302, redirectPath));
			return;
		} else {
			prepareResponse(conn, handleDirectoryRequest(conn, fullPath));
			return;
		}
	}

	// Handle file requests with trailing /
	_lggr.debug("File request: " + fullPath);
	if (ftype == ISREG && endSlash) {
		_lggr.debug("File request with trailing slash, redirecting: " + req.uri);
		std::string redirectPath = req.uri.substr(0, req.uri.length() - 1);
		prepareResponse(conn, handleReturnDirective(conn, 302, redirectPath));
		return;
	}

	// if we arrive here, this should be the only possible case
	if (ftype == ISREG && !endSlash) {

		_lggr.debug("File request with following extension: " + getExtension(req.uri));

		// check if it is a script with a language supported by the location
		if (conn->locConfig->acceptExtension(getExtension(req.path))) {
			std::string extPath = conn->locConfig->getExtensionPath(getExtension(req.path));
			_lggr.debug("Extension path is : " + extPath);
			req.extension = getExtension(req.path);
			if (!handleCGIRequest(req, conn)) {
				_lggr.logWithPrefix(Logger::ERROR, "BAD REQUEST", "Failed to invoke CGI.");
				prepareResponse(conn, Response::badRequest());
				// closeConnection(conn);
				return;
			}
			return;
		}

		if (req.method != "GET") {
			_lggr.debug("POST or DELETE request not handled by CGI -> not implemented response.");
			prepareResponse(conn, Response::notImplemented(conn));
			return;
		} else {
			prepareResponse(conn, handleFileRequest(conn, fullPath));
			return;
		}

		_lggr.debug("Should never be reached");
		prepareResponse(conn, Response::internalServerError(conn));
		return;
	}
}

bool WebServer::parseRequest(Connection *conn, ClientRequest &req) {
	_lggr.debug("Parsing request: " + conn->toString());
	if (!RequestParsingUtils::parse_request(vectorToString(conn->read_buffer), req)) {
		_lggr.logWithPrefix(Logger::ERROR, "BAD REQUEST", "Failed to parse the request.");
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

	case Connection::READING_BODY:
		_lggr.debug("isRequestComplete->READING_BODY");
		_lggr.debug(su::to_string(conn->content_length - conn->body_bytes_read) +
		            " bytes left to receive");
		if (static_cast<ssize_t>(conn->body_bytes_read) >= conn->content_length) {
			_lggr.debug("Read full content-length");
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

bool WebServer::isHeadersComplete(Connection *conn) {
	std::string buffer_str(conn->read_buffer.begin(), conn->read_buffer.end());
	size_t header_end = buffer_str.find("\r\n\r\n");
	if (header_end == std::string::npos) {
		return false;
	}

	// Headers are complete, check if this is a chunked request
	std::string headers = buffer_str.substr(0, header_end + 4);
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

		std::string remaining_data = buffer_str.substr(header_end + 4);
		buffer_str = remaining_data;

		conn->body_bytes_read = remaining_data.size();

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

			buffer_str.clear();

			conn->chunk_size = 0;
			conn->chunk_bytes_read = 0;
			conn->chunk_data.clear();

			return true;
		} else {
			conn->state = Connection::READING_CHUNK_SIZE;

			buffer_str = buffer_str.substr(header_end + 4);

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

bool WebServer::processChunkSize(Connection *conn) {
	size_t crlf_pos = findCRLF(vectorToString(conn->read_buffer));
	if (crlf_pos == std::string::npos) {
		// Need more data to read chunk size
		return false;
	}

	std::string chunk_size_line = vectorToString(conn->read_buffer).substr(0, crlf_pos);

	insertStringAt(conn->read_buffer, 0, vectorToString(conn->read_buffer).substr(crlf_pos + 2));

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
	size_t available_data = conn->read_buffer.size();
	size_t bytes_needed = conn->chunk_size - conn->chunk_bytes_read;

	if (available_data < bytes_needed + 2) { // +2 for trailing CRLF
		// Need more data
		return false;
	}

	size_t bytes_to_read = bytes_needed;
	std::string chunk_part = vectorToString(conn->read_buffer).substr(0, bytes_to_read);
	conn->chunk_data += chunk_part;
	conn->chunk_bytes_read += bytes_to_read;

	insertStringAt(conn->read_buffer, 0, vectorToString(conn->read_buffer).substr(bytes_to_read));

	// Check if there are trailing CRLF
	if (conn->read_buffer.size() < 2) {
		return false;
	}

	if (vectorToString(conn->read_buffer).substr(0, 2) != "\r\n") {
		_lggr.error("Invalid chunk format: missing trailing CRLF");
		return false;
	}

	// Remove trailing CRLF
	insertStringAt(conn->read_buffer, 0, vectorToString(conn->read_buffer).substr(2));

	conn->state = Connection::READING_CHUNK_SIZE;
	return processChunkSize(conn);
}

bool WebServer::processTrailer(Connection *conn) {
	size_t trailer_end = findCRLF(vectorToString(conn->read_buffer));

	if (trailer_end == std::string::npos) {
		// Need more data
		return false;
	}

	std::string trailer_line = vectorToString(conn->read_buffer).substr(0, trailer_end);
	insertStringAtEnd(conn->read_buffer, vectorToString(conn->read_buffer).substr(trailer_end + 2));

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

	insertStringAt(conn->read_buffer, 0, reconstructed_request + conn->chunk_data);
	conn->state = Connection::REQUEST_COMPLETE;

	_lggr.debug("Reconstructed chunked request, total body size: " +
	            su::to_string(conn->chunk_data.length()));
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
		    std::min(static_cast<size_t>(conn->content_length), conn->read_buffer.size());
		reconstructed_request += vectorToString(conn->read_buffer).substr(0, body_size);
	}

	insertStringAt(conn->read_buffer, 0, reconstructed_request);
	//_lggr.debug("Reconstructed request:\n" + conn->read_buffer);

	return true;
}

