/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EpollEventHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:06:48 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/19 19:17:12 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/HttpServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/Structs/WebServer.hpp"

void WebServer::processEpollEvents(const struct epoll_event *events, int event_count) {
	for (int i = 0; i < event_count; ++i) {
		const uint32_t event_mask = events[i].events;
		const int fd = events[i].data.fd;

		// _lggr.debug("Epoll event on fd=" + su::to_string(fd) + " (" +
		//             describeEpollEvents(event_mask) + ")");

		if (isListeningSocket(fd)) {
			// TODO: NULL check
			ServerConfig *sc = ServerConfig::find(_confs, fd);
			handleNewConnection(sc);
		} else if (isCGIFd(fd)) {
			handleCGIOutput(fd);
		} else {
			handleClientEvent(fd, event_mask);
		}
	}
}

bool WebServer::isListeningSocket(int fd) const {
	for (std::vector<ServerConfig>::const_iterator it = _confs.begin(); it != _confs.end(); ++it) {
		if (fd == it->getServerFD()) {
			return true;
		}
	}
	return false;
}

void WebServer::handleClientEvent(int fd, uint32_t event_mask) {
	std::map<int, Connection *>::iterator conn_it = _connections.find(fd);
	if (conn_it != _connections.end()) {
		Connection *conn = conn_it->second;
		if (event_mask & EPOLLIN) {
			handleClientRecv(conn);
		}
		if (event_mask & EPOLLOUT) {
			if (conn->response_ready)
				sendResponse(conn);
			if (!conn->keep_persistent_connection || conn->should_close)
				closeConnection(conn); // risk of closing the connection before the response is ready?
		}
		if (event_mask & (EPOLLERR | EPOLLHUP)) {
			_lggr.error("Error/hangup event for fd: " + su::to_string(fd));
			closeConnection(conn);
		}
	} else {
		_lggr.debug("Ignoring event for unknown fd: " + su::to_string(fd));
	}
}

void WebServer::handleClientRecv(Connection *conn) {
	_lggr.debug("Updated last activity for FD " + su::to_string(conn->fd));
	conn->updateActivity();

	char buffer[BUFFER_SIZE];

	ssize_t bytes_read = receiveData(conn->fd, buffer, sizeof(buffer) - 1);

	if (bytes_read > 0) {
		if (!processReceivedData(conn, buffer, bytes_read)) {
			return;
		}
	} else if (bytes_read == 0) {
		_lggr.warn("Client (fd: " + su::to_string(conn->fd) + ") closed connection");
		conn->keep_persistent_connection = false;
		closeConnection(conn);
		return;
	} else if (bytes_read < 0) {
		_lggr.error("recv error for fd " + su::to_string(conn->fd) + ": " + strerror(errno));
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

	_lggr.logWithPrefix(Logger::DEBUG, "recv", "Data: " + std::string(buffer));

	return bytes_read;
}

bool WebServer::processReceivedData(Connection *conn, const char *buffer, ssize_t bytes_read) {
	static int i = 0;

	_lggr.debug("MAX BODY : bytes read " + su::to_string( conn->body_bytes_read) 
			+ " / " + su::to_string(conn->getServerConfig()->getServerMaxBodySize()));
			
	if (conn->state == Connection::READING_HEADERS) {
		conn->read_buffer += std::string(buffer, bytes_read);
		std::cerr << i++ << " calls of processReceivedData (HEADERS)" << std::endl;
	} 
	
	else if (conn->state == Connection::READING_BODY) {
		conn->body_data.insert(conn->body_data.end(),
		                       reinterpret_cast<const unsigned char *>(buffer),
		                       reinterpret_cast<const unsigned char *>(buffer + bytes_read));
		conn->body_bytes_read += bytes_read;

		std::cerr << i++ << " calls of processReceivedData (BODY)" << std::endl;
		std::cerr << "Body data size: " << conn->body_data.size() << " bytes" << std::endl;

		_lggr.debug("Read " + su::to_string(conn->body_bytes_read) + " bytes of body so far");
	} 
	
	else {
		// For chunked data and other states, keep existing behavior
		conn->read_buffer += std::string(buffer, bytes_read);
		if (conn->state == Connection::READING_BODY) {
			conn->body_bytes_read += bytes_read;
		}
		std::cerr << i++ << " calls of processReceivedData (OTHER)" << std::endl;
	}

	_lggr.debug("Checking if request was completed");
	if (isRequestComplete(conn)) {
		if (!epollManage(EPOLL_CTL_MOD, conn->fd, EPOLLOUT)) {
			return false;
		}
		_lggr.debug("Request was completed");
		if (conn->chunked && conn->state == Connection::CONTINUE_SENT) {
			return true;
		}
		if (conn->should_close)
			return false;
		return handleCompleteRequest(conn);
	}

	return true;
}
