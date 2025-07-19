#include "HttpServer.hpp"
#include "src/Logger/Logger.hpp"
#include "src/RequestParser/request_parser.hpp"
#include "src/Utils/StringUtils.hpp"
#include <cerrno>
#include <cstdio>
#include <string>
#include <sys/socket.h>

void WebServer::handleClientData(int client_fd) {
	updateConnectionActivity(client_fd);

	std::map<int, ConnectionInfo *>::iterator conn_it = _connections.find(client_fd);
	if (conn_it == _connections.end()) {
		_lggr.error("No connection info found for fd: " + su::to_string(client_fd));
		closeConnection(client_fd);
		return;
	}

	ConnectionInfo *conn = conn_it->second;
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read;
	ssize_t total_bytes_read = 0;

	while (true) {
		errno = 0;
		bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
		total_bytes_read += bytes_read;

		if (bytes_read > 0) {
			_lggr.logWithPrefix(Logger::DEBUG, "recv loop",
			                    "Bytes read: " + su::to_string(bytes_read));
			buffer[bytes_read] = '\0';
			conn->buffer += std::string(buffer);
			if (isCompleteRequest(conn->buffer)) {
				if (total_bytes_read > _max_content_length) {
					_lggr.info("Reached max content length for fd: " + su::to_string(client_fd) +
					           ", " + su::to_string(bytes_read) + "/" +
					           su::to_string(_max_content_length));
					sendResponse(client_fd, Response(413)); // TODO: some tests of this part
					closeConnection(client_fd);
					return;
				}
				_lggr.logWithPrefix(Logger::DEBUG, "recv loop", "Request is complete");
				processRequest(client_fd, conn->buffer);
				if (_connections.find(client_fd) != _connections.end()) {
					conn->buffer.clear();
					conn->request_count++;
					conn->updateActivity();
					continue;
				} else {
					_lggr.debug("Client fd: " + su::to_string(client_fd) +
					            " wasn't found in the connections struct");
					return; // TODO: not sure if this is a good way to handle this
				}
			}
		} else if (bytes_read == 0) {
			_lggr.info("Client disconnected (fd: " + su::to_string(client_fd) + ")");
			closeConnection(client_fd);
			return;
		} else if (bytes_read < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) { // TODO: this is forbidden
				// No more data
				break;
			} else {
				_lggr.error("recv error for fd " + su::to_string(client_fd) + ": " +
				            strerror(errno));
				closeConnection(client_fd);
				return;
			}
		}
	}
}

void WebServer::processRequest(int client_fd, const std::string &raw_req) {
	_lggr.info("Processing request from fd " + su::to_string(client_fd) + ":");
	_lggr.info("Processing request from fd " + su::to_string(client_fd) + ":");
	_lggr.debug("--- Request Start ---\n" + raw_req);
	_lggr.debug("--- Request End ---");

	ClientRequest req;
	req.clfd = client_fd;
	if (!RequestParsingUtils::parse_request(raw_req, req)) {
		_lggr.error("Parsing of the request failed.");
		sendResponse(client_fd, Response::badRequest());
		closeConnection(client_fd);
		return;
	}
	std::string response;
	bool keep_alive = shouldKeepAlive(req);

	if (req.method == GET) {
		// TODO: do some check if handleGetRequest did not encounter any issues
		sendResponse(client_fd, handleGetRequest(req));
	} else if (req.method == POST || req.method == DELETE_) {
		sendResponse(client_fd, Response(501));
	} else {
		sendResponse(client_fd, Response::methodNotAllowed());
	}

	// TODO!: use new & awesome Response struct instead of all this shit
	// keep-alive shenanigans
	if (keep_alive) {
		// Insert keep-alive headers before the final \r\n\r\n
		size_t header_end = response.find("\r\n\r\n");
		if (header_end != std::string::npos) {
			std::string keep_alive_headers = "Connection: keep-alive\r\n"
			                                 "Keep-Alive: timeout=" +
			                                 su::to_string(KEEP_ALIVE_TO) +
			                                 ", max=" + su::to_string(MAX_KEEP_ALIVE_REQS) + "\r\n";
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
		_lggr.error("Failed to send response to client (fd: " + su::to_string(req.clfd) + ")");
	} else {
		_lggr.debug("Sent " + su::to_string(bytes_sent) + " bytes response to fd " +
		            su::to_string(req.clfd));
	}

	if (!keep_alive) {
		closeConnection(req.clfd);
	} else {
		updateConnectionActivity(req.clfd);
		_lggr.debug("Keeping connection alive for fd: " + su::to_string(req.clfd));
	}
}

bool WebServer::isCompleteRequest(const std::string &request) {
	// Simple check for HTTP request completion
	// Look for double CRLF which indicates end of headers
	// TODO: make it not simple, but a proper check
	return request.find("\r\n\r\n") != std::string::npos;
}

