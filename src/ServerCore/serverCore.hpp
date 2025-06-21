/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   serverCore.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 09:48:42 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/21 11:04:04 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef SERVERCORE_HPP
#define SERVERCORE_HPP

# include <iostream>
# include <cstring>
# include <unistd.h>
# include <sys/types.h> // socklen_t
# include <sys/socket.h> // core socket functions and struct
# include <netinet/in.h> // IPv4-specific definitions (sockaddr_in, INADDR_ANY, htons(), ...
# include <arpa/inet.h> // IP addr manip (inet_pton, ntohs, ...)
# include <sys/epoll.h>
# include <cstdio> 

# include "../Logger/Logger.hpp"
# include "../Utils/Utils.hpp"


namespace serverCreation {

	struct ServerConfig {
		
		std::string name;
		std::string host;
		int port;
		int backlog;
		int addrFamily;
		int socketType;
		int protocol;
		std::string root;

		ServerConfig() :	name("ServerExample"),
							host("0.0.0.0"), // INADDR_ANY (all interface) - "127.0.0.1" : loopback only
							port(8080),
							backlog(128), // Max pending connections
							addrFamily(AF_INET),
							socketType(SOCK_STREAM),
							protocol(0),			
							root("/var/www/html") {}
	} ;

	struct Server {
		ServerConfig	config;
		int 			fd;
	} ;


	// configuration
	ServerConfig parseConfig(std::string pathConfig);
	
	// creation
	int serverCreation(ServerConfig config, Logger &logger);
	int epollLoop(Server& server, Logger &logger);
	void handleServer(int epoll_fd, int server_fd, Logger &logger);
	void handleClient(int client_fd, Logger &logger);

}


#endif