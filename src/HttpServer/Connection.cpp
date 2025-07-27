#include "ConfigParser/config_parser.hpp"
#include "HttpServer.hpp"
#include "src/Logger/Logger.hpp"
#include "src/RequestParser/request_parser.hpp"
#include "src/Utils/StringUtils.hpp"
#include <cerrno>
#include <string>
#include <vector>

void WebServer::handleNewConnection(ServerConfig *sc) {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(sc->server_fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_fd == -1) {
		// TODO: cannot accept a connection with the client
	}

	if (!setNonBlocking(client_fd)) {
		close(client_fd);
		return;
	}

	// TODO: error checks
	Connection *conn = addConnection(client_fd, sc->host, sc->port);

	if (!epollManage(EPOLL_CTL_ADD, client_fd, EPOLLIN)) {
		closeConnection(conn);
	}

	_lggr.info("New connection from " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" +
	           su::to_string<unsigned short>(ntohs(client_addr.sin_port)) +
	           " (fd: " + su::to_string<int>(client_fd) + ")");
}

Connection *WebServer::addConnection(int client_fd, std::string host, int port) {
	Connection *conn = new Connection(client_fd);
	conn->host = host;
	conn->port = port;
	_connections[client_fd] = conn;

	_lggr.debug("Added connection tracking for fd: " + su::to_string(client_fd));
	return conn;
}

void WebServer::updateConnectionActivity(int client_fd) {
	std::map<int, Connection *>::iterator it = _connections.find(client_fd);
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
	for (std::map<int, Connection *>::iterator it = _connections.begin(); it != _connections.end();
	     ++it) {

		Connection *conn = it->second;
		if (conn->isExpired(time(NULL), CONNECTION_TO)) {
			_conn_to_close.push_back(conn->fd);
			_lggr.info("Connection expired for fd: " + su::to_string(conn->fd));
		}
	}

	// Close expired connections
	for (std::vector<int>::iterator it = _conn_to_close.begin(); it != _conn_to_close.end(); ++it) {
		handleConnectionTimeout(*it);
	}

	if (!_conn_to_close.empty()) {
		_lggr.info("Cleaned up " + su::to_string(_conn_to_close.size()) + " expired connections");
	}
}

void WebServer::handleConnectionTimeout(int client_fd) {
	std::map<int, Connection *>::iterator it = _connections.find(client_fd);
	if (it != _connections.end()) {
		Connection *conn = it->second;

		prepareResponse(conn, Response(408));

		_lggr.info("Connection timed out for fd: " + su::to_string(client_fd) + " (idle for " +
		           su::to_string(getCurrentTime() - conn->last_activity) + " seconds)");
		// closeConnection(conn);
		//  TODO: potential problems in case clfd not in connections for some reason
	}
}

bool WebServer::shouldKeepAlive(const ClientRequest &req) {
	// TODO: check if this could be improved using new & awesome Response struct
	std::map<int, Connection *>::iterator it = _connections.find(req.clfd);
	if (it == _connections.end()) {
		return false;
	}

	Connection *conn = it->second;

	if (conn->request_count >= MAX_KEEP_ALIVE_REQS) {
		_lggr.debug("Max keep-alive requests reached for fd: " + su::to_string(req.clfd));
		return false;
	}

	// Check for Connection: close header
	std::map<std::string, std::string>::const_iterator header_it = req.headers.find("connection");
	if (header_it != req.headers.end()) {
		_lggr.debug(" 123123 Found connection header for fd: " + su::to_string(req.clfd));
		std::string connection_value = header_it->second;
		std::transform(connection_value.begin(), connection_value.end(), connection_value.begin(),
		               ::tolower);

		if (connection_value == "close") {
			_lggr.debug("Closing connection for fd: " + su::to_string(req.clfd));
			return false;
		}
		if (connection_value == "keep-alive") {
			_lggr.debug("Keeping connection for fd: " + su::to_string(req.clfd) + " alive");
			return true;
		}
	}

	// Default keep-alive behavior for HTTP/1.1
	return req.version == "HTTP/1.1" || req.version == "HTTP/1.0";
}

void WebServer::closeConnection(Connection *conn) {
	if (!conn)
		return;
	if (conn->keep_alive && !conn->force_close) {
		_lggr.debug("Ignoring connection close request for fd: " + su::to_string(conn->fd) +
		            ", because of keep-alive");
		return;
	}
	_lggr.debug("Closing connection for fd: " + su::to_string(conn->fd));

	epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
	close(conn->fd);

	std::map<int, Connection *>::iterator it = _connections.find(conn->fd);
	if (it != _connections.end()) {
		_connections.erase(conn->fd);
	}
	_lggr.debug("Connection cleanup completed for fd: " + su::to_string(conn->fd));
	delete conn;
}

