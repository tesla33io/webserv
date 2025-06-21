/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   serverCreation.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 09:48:48 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/21 11:44:18 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "serverCore.hpp"

namespace serverCreation {

	int serverCreation(ServerConfig config, Logger &logger) {

		logger.debug("Creating server socket...");
		int serverFD = socket(config.addrFamily, config.socketType, config.protocol); 
		if (serverFD < 0) {
			logger.error("socket() failure: unable to create server socket.");
			return (-1);
		}
		
		sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr)); 
		addr.sin_family = config.addrFamily; 
		addr.sin_port = htons(config.port); // 2 bytes big endian

		logger.debug("Converting host IP string: " + config.host);
		if (inet_pton(config.addrFamily, config.host.c_str(), &addr.sin_addr) <= 0) {
			logger.critical("inet_pton() failed: invalid host IP '" + config.host + "'");
			close(serverFD);
			return (-1);
		}

		logger.debug("Binding to " + config.host + ":" + utils::toString(config.port));
		if (bind(serverFD, (sockaddr*)&addr, sizeof(addr)) < 0) {
			logger.critical("bind() failure: could not bind to " + config.host + ":" + utils::toString(config.port));
			close(serverFD);
			return (-1);
		}

		logger.debug("Setting socket to listen with backlog " + utils::toString(config.backlog));
		if (listen(serverFD, config.backlog) < 0) {
			logger.critical("listen() failure: could not start listening");
			close(serverFD);
			return (-1);
		}

		logger.info("[" + config.name + "]' is now listening on " + config.host + ":" + utils::toString(config.port));

		return (serverFD);
		
	}
}