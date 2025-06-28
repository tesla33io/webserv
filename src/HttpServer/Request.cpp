#include "HttpServer.hpp"
#include "src/Logger/Logger.hpp"
#include "src/RequestParser/request_parser.hpp"
#include "src/Utils/StringUtils.hpp"
#include <cerrno>

void WebServer::handleClientData(int client_fd) {
	updateConnectionActivity(client_fd);

	std::map<int, ConnectionInfo *>::iterator conn_it = _connections.find(client_fd);
	if (conn_it == _connections.end()) {
		_lggr.error("No connection info found for fd: " + string_utils::to_string(client_fd));
		closeConnection(client_fd);
		return;
	}

	ConnectionInfo *conn = conn_it->second;
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read;

	while (true) {
		errno = 0;
		bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

		if (bytes_read > 0) {
			_lggr.logWithPrefix(Logger::DEBUG, "recv loop",
			                    "Bytes read: " + string_utils::to_string(bytes_read));
			buffer[bytes_read] = '\0';
			conn->buffer += std::string(buffer);
			if (isCompleteRequest(conn->buffer)) {
				_lggr.logWithPrefix(Logger::DEBUG, "recv loop", "Request is complete");
				processRequest(client_fd, conn->buffer);
				conn->buffer.clear();
				conn->request_count++;
				updateConnectionActivity(client_fd);
				continue;
			}
		} else if (bytes_read == 0) {
			_lggr.info("Client disconnected (fd: " + string_utils::to_string(client_fd) + ")");
			closeConnection(client_fd);
			return;
		} else if (bytes_read < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// No more data
				break;
			} else {
				_lggr.error("recv error for fd " + string_utils::to_string(client_fd) + ": " +
				            strerror(errno));
				closeConnection(client_fd);
				return;
			}
		}
	}
}

void WebServer::processRequest(int client_fd, const std::string &raw_req) {
	_lggr.info("Processing request from fd " + string_utils::to_string(client_fd) + ":");
	_lggr.debug("--- Request Start ---\n" + raw_req);
	_lggr.debug("--- Request End ---");

	ClientRequest req;
	req.clfd = client_fd;
	if (!RequestParsingUtils::parse_request(raw_req, req)) {
		// TODO: handle this error
		_lggr.error("Parsing of the request failed.");
		return;
	}

	// Send a simple HTTP response
	sendResponse(req);
}

bool WebServer::isCompleteRequest(const std::string &request) {
	// Simple check for HTTP request completion
	// Look for double CRLF which indicates end of headers
	return request.find("\r\n\r\n") != std::string::npos;
}

void WebServer::sendResponse(const ClientRequest &req) {
	std::string response;
	bool keep_alive = shouldKeepAlive(req);

	if (req.method == GET) {
		response = handleGetRequest(_root_path + req.uri);
	} else if (req.method == POST || req.method == DELETE_) {
		response = generateErrorResponse(501);
	} else {
		response = generateErrorResponse(405);
	}

	// keep-alive shenanigans
	if (keep_alive) {
		// Insert keep-alive headers before the final \r\n\r\n
		size_t header_end = response.find("\r\n\r\n");
		if (header_end != std::string::npos) {
			std::string keep_alive_headers = "Connection: keep-alive\r\n"
			                                 "Keep-Alive: timeout=" +
			                                 string_utils::to_string(KEEP_ALIVE_TO) + ", max=" +
			                                 string_utils::to_string(MAX_KEEP_ALIVE_REQS) + "\r\n";

			response.insert(header_end, keep_alive_headers);
		}
	} else {
		// Add connection close header
		size_t header_end = response.find("\r\n\r\n");
		if (header_end != std::string::npos) {
			response.insert(header_end, "Connection: close\r\n");
		}
	}
	ssize_t bytes_sent = send(req.clfd, response.c_str(), response.size(), 0);
	if (bytes_sent < 0) {
		_lggr.error("Failed to send response to client (fd: " + string_utils::to_string(req.clfd) +
		            ")");
	} else {
		_lggr.debug("Sent " + string_utils::to_string(bytes_sent) + " bytes response to fd " +
		            string_utils::to_string(req.clfd));
	}

	if (!keep_alive) {
		closeConnection(req.clfd);
	} else {
		updateConnectionActivity(req.clfd);
		_lggr.debug("Keeping connection alive for fd: " + string_utils::to_string(req.clfd));
	}
}

