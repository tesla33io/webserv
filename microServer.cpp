/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   miniServer_example.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/17 16:41:53 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/21 09:51:40 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/*******************************************************************************
int socket(int domain, int type, int protocol); create a socket, returns the fd
  - domain : AF_INET: IPv4, AF_INET6:IPv6, AF_UNIX or AF_LOCAL: Local communica-
			 tion (<sys/socket.h>)
  - type : SOCK_STREAM for TCP, SOCK_DGRAM for UDP, SOCK_SEQPACKET: for records 
		   (rarely used), SOCK_RAW: raw network protocol access (advanced)
  - protocol: 0 for default protocol for the give domain and type

struct sockaddr_in {
	  short            sin_family;   // must match otherwise EINVAL in bind,...
	  unsigned short   sin_port;     // host to network short (big endian)
	  struct in_addr   sin_addr;     // IP address (in network byte order)
	  char             sin_zero[8];  // Padding (not used, just for alignment)
  };
  
After creating the socket
- bind it to a specific IP address and port (bind())
- listen for incoming connections (listen())


- accept connections from clients (accept())

*******************************************************************************/

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>

int main() {
	
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) return perror("socket"), 1;

	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr)); 
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(8080);

	if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
		return perror("bind"), close(server_fd), 1;

	if (listen(server_fd, 5) < 0)
		return perror("listen"), close(server_fd), 1;

	std::cout << "Serving HTTP on port 8080\n";

	while (true) {
		
		sockaddr_in client_addr;
		std::memset(&client_addr, 0, sizeof(client_addr)); 
		socklen_t client_len = sizeof(client_addr);
		// accept is blocking
		int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
		if (client_fd < 0) continue;

		char buffer[1024];
		// recv is blocking
		ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
		if (n > 0) {
			buffer[n] = '\0';
			std::cout << buffer;

		const char *response =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 27\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<h1>Hello, world!</h1>";
			send(client_fd, response, strlen(response), 0);
		}
		close(client_fd);
	}
}
