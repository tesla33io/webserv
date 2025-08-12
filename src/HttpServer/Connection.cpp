/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:09:35 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/12 17:42:29 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpServer.hpp"

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
	Connection *conn = addConnection(client_fd, sc);

	if (!epollManage(EPOLL_CTL_ADD, client_fd, EPOLLIN)) {
		closeConnection(conn);
	}

	_lggr.info("New connection from " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" +
	           su::to_string<unsigned short>(ntohs(client_addr.sin_port)) +
	           " (fd: " + su::to_string<int>(client_fd) + ")");
}

Connection *WebServer::addConnection(int client_fd, ServerConfig *sc) {
	Connection *conn = new Connection(client_fd);
	// conn->host = host;
	// conn->port = port;
	conn->servConfig = sc;
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

// SEND THE 408 here?
void WebServer::cleanupExpiredConnections() {
	time_t current_time = getCurrentTime();

	if (current_time - _last_cleanup < CLEANUP_INTERVAL) {
		return;
	}

	_last_cleanup = current_time;

	std::vector<Connection *> expired;

	// Collect expired connections
	for (std::map<int, Connection *>::iterator it = _connections.begin(); it != _connections.end();
	     ++it) {

		Connection *conn = it->second;
		if (conn->isExpired(time(NULL), CONNECTION_TO)) {
			conn->keep_persistent_connection = false;
			expired.push_back(conn);
			_lggr.info("Connection expired for fd: " + su::to_string(conn->fd));
		}
	}

	for (size_t i = 0; i < expired.size(); ++i) {
		handleConnectionTimeout(expired[i]->fd);
	}
	expired.clear();
}


// TODO: deprecated? not called from anywhere 
void WebServer::handleConnectionTimeout(int client_fd) {
	std::map<int, Connection *>::iterator it = _connections.find(client_fd);
	if (it != _connections.end()) {
		Connection *conn = it->second;

		prepareResponse(conn, Response(408, conn));

		_lggr.info("Connection timed out for fd: " + su::to_string(client_fd) + " (idle for " +
		           su::to_string(getCurrentTime() - conn->last_activity) + " seconds)");
		closeConnection(conn);
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

	// TODO: redundant check may be removed
	if (conn->keep_persistent_connection) {
		_lggr.debug("Ignoring connection close request for fd: " + su::to_string(conn->fd));
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
