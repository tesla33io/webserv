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
#include <sys/socket.h>

void WebServer::handleClientData(int client_fd) {
	_lggr.debug("Updated last activity for FD " + su::to_string(client_fd));
	updateConnectionActivity(client_fd);

	ConnectionInfo *conn = getConnectionInfo(client_fd);
	if (!conn)
		return;

	char buffer[BUFFER_SIZE];
	ssize_t total_bytes_read = 0;

	_lggr.debug("Starting recieving loop");
	while (true) {
		errno = 0;
		ssize_t bytes_read = receiveData(client_fd, buffer, sizeof(buffer) - 1);

		if (bytes_read > 0) {
			total_bytes_read += bytes_read;

			if (!processReceivedData(client_fd, conn, buffer, bytes_read, total_bytes_read)) {
				return;
			}
		} else if (bytes_read == 0) {
			handleClientDisconnection(conn);
			return;
		} else if (bytes_read < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) { // TODO: this is forbidden
				// No more data
				break;
			} else {
				_lggr.error("recv error for fd " + su::to_string(client_fd) + ": " +
				            strerror(errno));
				closeConnection(conn);
				return;
			}
		}
	}
}

ssize_t WebServer::receiveData(int client_fd, char *buffer, size_t buffer_size) {
	errno = 0;
	ssize_t bytes_read = recv(client_fd, buffer, buffer_size, 0);

	if (bytes_read > 0) {
		_lggr.logWithPrefix(Logger::DEBUG, "recv loop", "Bytes read: " + su::to_string(bytes_read));
		buffer[bytes_read] = '\0';
	}

	return bytes_read;
}

bool WebServer::processReceivedData(int client_fd, ConnectionInfo *conn, const char *buffer,
                                    ssize_t bytes_read, ssize_t total_bytes_read) {
	conn->buffer += std::string(buffer);
	_lggr.debug("FD " + su::to_string(client_fd) + "\n" + conn->toString());

	if (isCompleteRequest(conn)) {
		// TODO: remove hard-coded limit
		if (total_bytes_read > 4096) {
			handleRequestTooLarge(conn, bytes_read);
			return false;
		}

		return handleCompleteRequest(client_fd, conn);
	}

	return true;
}

void WebServer::handleClientDisconnection(ConnectionInfo *conn) {
	_lggr.info("Client disconnected (fd: " + su::to_string(conn->clfd) + ")");
	closeConnection(conn);
}

void WebServer::handleRequestTooLarge(ConnectionInfo *conn, ssize_t bytes_read) {
	_lggr.info("Reached max content length for fd: " + su::to_string(conn->clfd) + ", " +
	           su::to_string(bytes_read) + "/" + su::to_string(4096));
	sendResponse(conn->clfd, Response(413));
	closeConnection(conn);
}

bool WebServer::handleCompleteRequest(int client_fd, ConnectionInfo *conn) {
	_lggr.logWithPrefix(Logger::DEBUG, "recv loop", "Request is complete");
	processRequest(client_fd, conn);

	if (_connections.find(client_fd) != _connections.end()) {
		conn->buffer.clear();
		conn->request_count++;
		conn->updateActivity();
		return true; // Continue processing
	} else {
		_lggr.debug("Client fd: " + su::to_string(client_fd) +
		            " wasn't found in the connections struct");
		return false; // Stop processing
	}
}

void WebServer::processRequest(int client_fd, ConnectionInfo *conn) {
	_lggr.info("Processing request from fd " + su::to_string(client_fd) + ":");

	ClientRequest req;
	req.clfd = client_fd;

	if (!parseRequest(conn, req))
		return;
	_lggr.debug("Request parsed sucsessfully");

	if (req.chunked_encoding && conn->state == ConnectionInfo::READING_HEADERS) {
		// Accept chunked requests sequence
		_lggr.debug("Accepting a chunked request");
		conn->state = ConnectionInfo::READING_CHUNK_SIZE;
		conn->chunked = true;
		sendResponse(client_fd, Response::continue_());
		return;
	}

	// TODO: this part breaks the req struct for some reason
	//       can't debug on my own :(
	if (req.chunked_encoding && conn->state == ConnectionInfo::CHUNK_COMPLETE) {
		_lggr.debug("Chunked request completed!");
		_lggr.debug("Parsing complete chunked request");
		if (!parseRequest(conn, req))
			return;
		_lggr.debug("Chunked request parsed sucsessfully");
		_lggr.debug(conn->toString());
		_lggr.debug(req.toString());
	}

	_lggr.debug("FD " + su::to_string(req.clfd) + " ClientRequest {\n" + req.toString() + "}");
	std::string response;

	if (req.method == "GET") {
		// TODO: do some check if handleGetRequest did not encounter any issues
		sendResponse(client_fd, handleGetRequest(req));
	} else if (req.method == "POST" || req.method == "DELETE_") {
		sendResponse(client_fd, Response(501));
	} else {
		sendResponse(client_fd, Response::methodNotAllowed());
	}
}

bool WebServer::parseRequest(ConnectionInfo *conn, ClientRequest &req) {
	_lggr.debug("Parsing request: " + conn->toString());
	if (!RequestParsingUtils::parse_request(conn->buffer, req)) {
		_lggr.error("Parsing of the request failed.");
		_lggr.debug("FD " + su::to_string(conn->clfd) + "\n" + conn->toString());
		sendResponse(conn->clfd, Response::badRequest());
		closeConnection(conn);
		return false;
	}
	return true;
}

bool WebServer::isCompleteRequest(ConnectionInfo *conn) {
	// Simple check for HTTP request completion
	// Look for double CRLF which indicates end of headers
	// TODO: make it not simple, but a proper check
	return conn->buffer.find("\r\n\r\n") != std::string::npos;
}

WebServer::ConnectionInfo *WebServer::getConnectionInfo(int client_fd) {
	std::map<int, ConnectionInfo *>::iterator conn_it = _connections.find(client_fd);
	if (conn_it == _connections.end()) {
		_lggr.error("No connection info found for fd: " + su::to_string(client_fd));
		closeConnection(conn_it->second);
		return NULL;
	}
	return conn_it->second;
}

