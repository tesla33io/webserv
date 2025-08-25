/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:09:35 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/25 14:57:34 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/Structs/WebServer.hpp"

void WebServer::handleNewConnection(ServerConfig *sc) {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(sc->getServerFD(), (struct sockaddr *)&client_addr, &client_len);
	if (client_fd == -1) {
		return;
	}

	if (!setNonBlocking(client_fd)) {
		close(client_fd);
		return;
	}

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
	conn->servConfig = sc;
	_connections[client_fd] = conn;

	_lggr.debug("Added connection tracking for fd: " + su::to_string(client_fd));
	return conn;
}

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

void WebServer::closeConnection(Connection *conn) {
	if (!conn)
		return;

	_lggr.debug("Closing connection for fd: " + su::to_string(conn->fd));

	std::map<int, Connection *>::iterator it = _connections.find(conn->fd);
	if (it == _connections.end()) {
		_lggr.debug("Connection already closed for fd: " + su::to_string(conn->fd));
		return;
	}
	if (it->second != conn) {
		_lggr.error("Connection object mismatch for fd: " + su::to_string(conn->fd));
		return;
	}
	epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
	close(conn->fd);
	_connections.erase(it);
	_lggr.debug("Connection cleanup completed for fd: " + su::to_string(conn->fd));
	delete conn;
}
