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

void WebServer::handleClientRecv(Connection *conn) {
	_lggr.debug("Updated last activity for FD " + su::to_string(conn->clfd));
	conn->updateActivity();

	char buffer[BUFFER_SIZE];
	ssize_t total_bytes_read = 0;

	ssize_t bytes_read = receiveData(conn->clfd, buffer, sizeof(buffer) - 1);

	if (bytes_read > 0) {
		total_bytes_read += bytes_read;

		if (!processReceivedData(conn, buffer, bytes_read, total_bytes_read)) {
			return;
		}
	} else if (bytes_read == 0) {
		handleClientDisconnection(conn);
		return;
	} else if (bytes_read < 0) {
		_lggr.error("recv error for fd " + su::to_string(conn->clfd) + ": " + strerror(errno));
		closeConnection(conn);
		return;
	}
}

ssize_t WebServer::receiveData(int client_fd, char *buffer, size_t buffer_size) {
	errno = 0;
	ssize_t bytes_read = recv(client_fd, buffer, buffer_size, 0);

	_lggr.logWithPrefix(Logger::DEBUG, "recv", "Bytes received: " + su::to_string(bytes_read));
	if (bytes_read > 0) {
		buffer[bytes_read] = '\0';
	}

	return bytes_read;
}

bool WebServer::processReceivedData(Connection *conn, const char *buffer, ssize_t bytes_read,
                                    ssize_t total_bytes_read) {
	conn->read_buffer += std::string(buffer);

	_lggr.debug("Checking if request was completed");
	if (isCompleteRequest(conn)) {
		if (!epollManage(EPOLL_CTL_MOD, conn->clfd, EPOLLIN | EPOLLOUT)) {
			return false;
		}
		_lggr.debug("Request was completed");
		// TODO: remove hard-coded limit
		if (total_bytes_read > 4096) {
			_lggr.debug("Request is too large");
			handleRequestTooLarge(conn, bytes_read);
			return false;
		}

		return handleCompleteRequest(conn->clfd, conn);
	}

	return true;
}

void WebServer::handleClientDisconnection(Connection *conn) {
	_lggr.info("Client disconnected (fd: " + su::to_string(conn->clfd) + ")");
	closeConnection(conn);
}

void WebServer::handleRequestTooLarge(Connection *conn, ssize_t bytes_read) {
	_lggr.info("Reached max content length for fd: " + su::to_string(conn->clfd) + ", " +
	           su::to_string(bytes_read) + "/" + su::to_string(4096));
	prepareResponse(conn, Response(413));
	// closeConnection(conn);
}

bool WebServer::handleCompleteRequest(int client_fd, Connection *conn) {
	processRequest(conn);

	if (_connections.find(client_fd) != _connections.end()) {
		conn->read_buffer.clear();
		conn->request_count++;
		conn->updateActivity();
		return true; // Continue processing
	} else {
		_lggr.debug("Client fd: " + su::to_string(client_fd) +
		            " wasn't found in the connections struct");
		return false; // Stop processing
	}
}

void WebServer::processRequest(Connection *conn) {
	_lggr.info("Processing request from fd: " + su::to_string(conn->clfd));

	ClientRequest req;
	req.clfd = conn->clfd;

	if (!parseRequest(conn, req))
		return;
	_lggr.debug("Request parsed sucsessfully");

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

	if (req.method == "GET") {
		// TODO: do some check if handleGetRequest did not encounter any issues
		prepareResponse(conn, handleGetRequest(req));
	} else if (req.method == "POST" || req.method == "DELETE_") {
		prepareResponse(conn, Response(501));
	} else {
		prepareResponse(conn, Response::methodNotAllowed());
	}
}

bool WebServer::parseRequest(Connection *conn, ClientRequest &req) {
	_lggr.debug("Parsing request: " + conn->toString());
	if (!RequestParsingUtils::parse_request(conn->read_buffer, req)) {
		_lggr.error("Parsing of the request failed.");
		_lggr.debug("FD " + su::to_string(conn->clfd) + " " + conn->toString());
		prepareResponse(conn, Response::badRequest());
		// closeConnection(conn);
		return false;
	}
	return true;
}

bool WebServer::isCompleteRequest(Connection *conn) {
	// Simple check for HTTP request completion
	// Look for double CRLF which indicates end of headers
	// TODO: make it not simple, but a proper check
	return conn->read_buffer.find("\r\n\r\n") != std::string::npos;
}

WebServer::Connection *WebServer::getConnection(int client_fd) {
	std::map<int, Connection *>::iterator conn_it = _connections.find(client_fd);
	if (conn_it == _connections.end()) {
		_lggr.error("No connection info found for fd: " + su::to_string(client_fd));
		close(client_fd);
		return NULL;
	}
	return conn_it->second;
}

