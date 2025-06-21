/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 09:48:45 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/21 11:09:05 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "serverCore.hpp"

int main(int argc, char **argv) {

	Logger logger("webserver.log", Logger::DEBUG, true);

	serverCreation::ServerConfig config;
	
	if (argc == 2) 
		config = serverCreation::parseConfig(argv[1]);

	else if (argc != 1) {
		logger.error("Usage: ./webserv [opt: configuration_file]");
		return (1);
	}
			
	int serverFD = serverCreation::serverCreation(config, logger);

	serverCreation::Server server;
	server.config = config;
	server.fd = serverFD;

	serverCreation::epollLoop(server, logger);

	close(serverFD);
	logger.info("The server has clsoed.");
	
	return (0);
}

