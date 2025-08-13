/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:10:22 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 14:17:07 by jalombar         ###   ########.fr       */
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

/* Request processing */

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
				_lggr.error("Handling the CGI request failed.");
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
	if (!RequestParsingUtils::parseRequest(conn->read_buffer, req)) {
		_lggr.error("Parsing of the request failed.");
		_lggr.debug("FD " + su::to_string(conn->fd) + " " + conn->toString());
		prepareResponse(conn, Response::badRequest(conn));
		// closeConnection(conn);
		return false;
	}
	return true;
}
