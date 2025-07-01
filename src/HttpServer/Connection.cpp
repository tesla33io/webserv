#include "HttpServer.hpp"
#include "src/Logger/Logger.hpp"
#include "src/RequestParser/request_parser.hpp"
#include "src/Utils/StringUtils.hpp"
#include <cerrno>

void WebServer::handleNewConnection() {
	while (true) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int client_fd = accept(_server_fd, (struct sockaddr *)&client_addr, &client_len);
		if (client_fd == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// No more pending connections
				return;
			} else {
				_lggr.error("Failed to accept connection");
				return;
			}
		}

		if (!setNonBlocking(client_fd)) {
			close(client_fd);
			continue;
		}

		struct epoll_event ev;
		ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
		ev.data.fd = client_fd;

		if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
			_lggr.error("Failed to add client socket (fd: " +
			            su::to_string<int>(client_fd) + ") to epoll");
			close(client_fd);
			continue;
		}

		// Initialize client buffer
		addConnection(client_fd);

		_lggr.info("New connection from " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" +
		           su::to_string<unsigned short>(ntohs(client_addr.sin_port)) +
		           " (fd: " + su::to_string<int>(client_fd) + ")");
	}
}

void WebServer::addConnection(int client_fd) {
	ConnectionInfo *conn = new ConnectionInfo(client_fd);
	_connections[client_fd] = conn;

	_lggr.debug("Added connection tracking for fd: " + su::to_string(client_fd));
}

void WebServer::updateConnectionActivity(int client_fd) {
	std::map<int, ConnectionInfo *>::iterator it = _connections.find(client_fd);
	if (it != _connections.end()) {
		it->second->last_activity = getCurrentTime();
		_lggr.debug("Updated activity for fd: " + su::to_string(client_fd));
	}
}

void WebServer::cleanupExpiredConnections() {
	time_t current_time = getCurrentTime();

	if (current_time - _last_cleanup < CLEANUP_INTERVAL) {
		return;
	}

	_last_cleanup = current_time;
	_conn_to_close.clear();

	// Collect expired connections
	for (std::map<int, ConnectionInfo *>::iterator it = _connections.begin();
	     it != _connections.end(); ++it) {

		ConnectionInfo *conn = it->second;
		if (isConnectionExpired(conn)) {
			_conn_to_close.push_back(conn->clfd);
			_lggr.info("Connection expired for fd: " + su::to_string(conn->clfd));
		}
	}

	// Close expired connections
	for (std::vector<int>::iterator it = _conn_to_close.begin(); it != _conn_to_close.end(); ++it) {
		handleConnectionTimeout(*it);
	}

	if (!_conn_to_close.empty()) {
		_lggr.info("Cleaned up " + su::to_string(_conn_to_close.size()) +
		           " expired connections");
	}
}

void WebServer::handleConnectionTimeout(int client_fd) {
	std::map<int, ConnectionInfo *>::iterator it = _connections.find(client_fd);
	if (it != _connections.end()) {
		ConnectionInfo *conn = it->second;

		// Send timeout response if connection is still active
		std::string timeout_response = generateErrorResponse(408);
		send(client_fd, timeout_response.c_str(), timeout_response.size(), 0);

		_lggr.info("Connection timed out for fd: " + su::to_string(client_fd) +
		           " (idle for " + su::to_string(getCurrentTime() - conn->last_activity) +
		           " seconds)");
	}

	closeConnection(client_fd);
}

bool WebServer::shouldKeepAlive(const ClientRequest &req) {
	std::map<int, ConnectionInfo *>::iterator it = _connections.find(req.clfd);
	if (it == _connections.end()) {
		return false;
	}

	ConnectionInfo *conn = it->second;

	if (conn->request_count >= MAX_KEEP_ALIVE_REQS) {
		_lggr.debug("Max keep-alive requests reached for fd: " + su::to_string(req.clfd));
		return false;
	}

	// Check for Connection: close header
	std::map<std::string, std::string>::const_iterator header_it = req.headers.find("connection");
	if (header_it != req.headers.end()) {
		std::string connection_value = header_it->second;
		// Convert to lowercase for comparison
		std::transform(connection_value.begin(), connection_value.end(), connection_value.begin(),
		               ::tolower);

		if (connection_value == "close") {
			return false;
		}
		if (connection_value == "keep-alive") {
			return true;
		}
	}

	// Default keep-alive behavior for HTTP/1.1
	return req.version == "HTTP/1.1";
}

void WebServer::closeConnection(int client_fd) {
	_lggr.debug("Closing connection for fd: " + su::to_string(client_fd));

	epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
	close(client_fd);

	std::map<int, ConnectionInfo *>::iterator it = _connections.find(client_fd);
	if (it != _connections.end()) {
		if (it != _connections.end()) {
			ConnectionInfo *info = it->second;
			_connections.erase(it);
			delete info;
		}
	}

	_lggr.debug("Connection cleanup completed for fd: " + su::to_string(client_fd));
}

