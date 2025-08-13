/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:09:35 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 14:19:08 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"

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
		closeConnection(expired[i]);
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
