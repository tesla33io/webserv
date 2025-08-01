#include "../HttpServer.hpp"
#include <cstring>
#include <string>

void WebServer::processEpollEvents(const struct epoll_event *events, int event_count) {
	for (int i = 0; i < event_count; ++i) {
		const uint32_t event_mask = events[i].events;
		const int fd = events[i].data.fd;

		_lggr.debug("Epoll event on fd=" + su::to_string(fd) + " (" +
		            describeEpollEvents(event_mask) + ")");

		if (isListeningSocket(fd)) {
			// TODO: NULL check
			ServerConfig *sc = ServerConfig::find(_confs, fd);
			handleNewConnection(sc);
		} else {
			handleClientEvent(fd, event_mask);
		}
	}
}

bool WebServer::isListeningSocket(int fd) const {
	for (std::vector<ServerConfig>::const_iterator it = _confs.begin(); it != _confs.end(); ++it) {
		if (fd == it->server_fd) {
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
			if (!conn->keep_persistent_connection)
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
	ssize_t total_bytes_read = 0;

	ssize_t bytes_read = receiveData(conn->fd, buffer, sizeof(buffer) - 1);

	if (bytes_read > 0) {
		total_bytes_read += bytes_read;

		if (!processReceivedData(conn, buffer, bytes_read, total_bytes_read)) {
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

bool WebServer::processReceivedData(Connection *conn, const char *buffer, ssize_t bytes_read,
                                    ssize_t total_bytes_read) {
	conn->read_buffer += std::string(buffer);

	_lggr.debug("Checking if request was completed");
	if (isRequestComplete(conn)) {
		if (!epollManage(EPOLL_CTL_MOD, conn->fd, EPOLLIN | EPOLLOUT)) {
			return false;
		}
		_lggr.debug("Request was completed");
		if (conn->chunked && conn->state == Connection::CONTINUE_SENT) {
			return true;
		}
		// TODO: remove hard-coded limit
		if (total_bytes_read > 4096) {
			_lggr.debug("Request is too large");
			handleRequestTooLarge(conn, bytes_read);
			return false;
		}

		return handleCompleteRequest(conn);
	}

	return true;
}

