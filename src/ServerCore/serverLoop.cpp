/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   serverLoop.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 09:48:55 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/21 11:42:46 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "serverCore.hpp"

namespace serverCreation {

	int epollLoop(Server& server, Logger &logger);
	void handleServer(int epoll_fd, int server_fd, Logger &logger);
	void handleClient(int client_fd, Logger &logger);

	int epollLoop(Server& server, Logger &logger) {

		const int maxEvent = 10;
		epoll_event ev;
		epoll_event events[maxEvent];

		logger.debug("Creating epoll instance for server: " + server.config.name);
		int epoll_fd = epoll_create1(0);
		if (epoll_fd == -1) {
			logger.error("epoll_create1() failed: could not create epoll instance");
			return -1;
		}
		logger.info("[" + server.config.name + "] epoll instance created | "
		"server_fd = " + utils::toString(server.fd) + ", "
		"epoll_fd = " + utils::toString(epoll_fd) + ", "
		"listening on " + server.config.host + ":" + utils::toString(server.config.port)
	);

		logger.debug("Adding server fd " + utils::toString(server.fd) + " to epoll watch list");
		ev.data.fd = server.fd;
		ev.events = EPOLLIN;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server.fd, &ev) == -1) {
			logger.error("epoll_ctl() failed: could not add server fd to watch list");
			return -1;
		}
		logger.info("Started epoll monitoring on server fd");

		while (1) {
			
			int nReady = epoll_wait(epoll_fd, events, maxEvent, -1);
			if (nReady == -1) {
				logger.error("epoll_wait() failure: could not poll file descriptors");
				return -1;
			}

			if (logger.isLevelEnabled(Logger::DEBUG)) {
				logger.debug("epoll_wait() returned " + utils::toString(nReady) + " ready file descriptor(s)");
			}

			for (int i = 0; i < nReady; ++i) {
				
				int fd = events[i].data.fd;
				
				if (fd == server.fd) {
					handleServer(epoll_fd, server.fd, logger);
				} else {
					handleClient(fd, logger);
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL); // DEL: stop monitoring before closing
				}
			}
		}
	}

	void handleServer(int epoll_fd, int server_fd, Logger &logger) {

		sockaddr_in client_addr;
		std::memset(&client_addr, 0, sizeof(client_addr));
		socklen_t client_len = sizeof(client_addr);
		int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len); // complete the handshake
		if (client_fd < 0) {
			logger.error("accept() failed: unable to accept client connection");
			return;
		}

		logger.info("Accepted new client connection (fd = " + utils::toString(client_fd) + ")");

		epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = client_fd;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
			logger.warn("epoll_ctl() failed: could not add client fd " + utils::toString(client_fd) + " to watch list");
			close(client_fd);
		}
	}

	void handleClient(int client_fd, Logger &logger) {

		char buffer[1024];
		ssize_t count = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
		if (count == 0) {
			logger.info("Client disconnected (fd = " + utils::toString(client_fd) + ")");
			close(client_fd);
			return;
		}

		if (count < 0) {
			logger.error("recv() failed on fd " + utils::toString(client_fd));
			close(client_fd);
			return;
		}

		buffer[count] = '\0';
		logger.info("Request received from fd " + utils::toString(client_fd));

		if (logger.isLevelEnabled(Logger::DEBUG)) {
			logger.debug("Raw HTTP request from fd " + utils::toString(client_fd) + ":\n" + std::string(buffer));
		}

		const char* response =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 27\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<h1>Hello, world!</h1>";
		send(client_fd, response, strlen(response), 0);
		logger.info("Response sent, closing connection (fd = " + utils::toString(client_fd) + ")");
		close(client_fd);
	}
}