#include "../HttpServer.hpp"
#include <cstring>
#include <string>

void print_cgi_response(const std::string &cgi_output) {
	std::istringstream response_stream(cgi_output);
	std::string line;
	bool in_body = false;

	while (std::getline(response_stream, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		if (!in_body && line.empty()) {
			in_body = true;
			std::cout << std::endl;
			continue;
		}

		std::cout << line << std::endl;
	}
}

void WebServer::sendCGIResponse(std::string &cgi_output, CGI *cgi, Connection *conn) {
	Response resp;
	resp.setStatus(200);
	resp.version = "HTTP/1.1";
	resp.setContentType(cgi->extract_content_type(cgi_output));

	size_t header_end = cgi_output.find("\n\n");
	if (header_end != std::string::npos) {
		// Found header separator, extract body after it
		resp.body = cgi_output.substr(header_end + 2); // +2 to skip "\n\n"
		resp.setContentLength(resp.body.length());
	} else {
		// No header separator found, treat entire output as body
		resp.body = cgi_output;
		resp.setContentLength(cgi_output.length());
	}

	std::string raw_response = resp.toString();
	conn->response_ready = true;
	send(conn->fd, raw_response.c_str(), raw_response.length(), 0);
	cgi->cleanup();
}

void WebServer::chunkedResponse(CGI *cgi, Connection *conn) {
	(void)cgi;
	(void)conn;
}

void WebServer::normalResponse(CGI *cgi, Connection *conn) {
	Logger logger;
	std::string cgi_output;
	char buffer[4096];
	ssize_t bytes_read;

	while ((bytes_read = read(cgi->getOutputFd(), buffer, sizeof(buffer))) > 0) {
		cgi_output.append(buffer, bytes_read);
	}

	if (bytes_read == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Error reading from CGI script");
		close(cgi->getOutputFd());
		waitpid(cgi->getPid(), NULL, 0);
		return;
	}
	print_cgi_response(cgi_output);
	sendCGIResponse(cgi_output, cgi, conn);
}

void WebServer::handleCGIOutput(int fd) {
	bool chunked = false;
	CGI *cgi;
	Connection *conn;
	for (std::map<int, std::pair<CGI *, Connection *> >::iterator it = _cgi_pool.begin();
	     it != _cgi_pool.end(); ++it) {
		if (fd == it->first) {
			cgi = it->second.first;
			conn = it->second.second;
		}
	}
	if (chunked)
		chunkedResponse(cgi, conn);
	else
		normalResponse(cgi, conn);
}

bool WebServer::isCGIFd(int fd) const { return (_cgi_pool.find(fd) != _cgi_pool.end()); }

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
		} else if (isCGIFd(fd)) {
			handleCGIOutput(fd);
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
		if (!conn->getServerConfig()->infiniteBodySize()
	 		      && total_bytes_read > static_cast<ssize_t>(conn->getServerConfig()->getMaxBodySize())) {
			_lggr.debug("Request is too large");
			handleRequestTooLarge(conn, bytes_read);
			return false;
		}

		return handleCompleteRequest(conn);
	}

	return true;
}

