/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerCGI.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 11:38:44 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/23 23:43:00 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/HttpServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/Structs/WebServer.hpp"

void printCGIResponse(const std::string &cgi_output) {
	std::istringstream response_stream(cgi_output);
	std::string line;
	bool in_body = false;

	std::cout << "PRINTING CGI OUTPUT: \n";
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

bool WebServer::prepareCGIResponse(CGI *cgi, Connection *conn) {
	Logger logger;
	std::string cgi_output;
	char buffer[4096];
	ssize_t bytes_read;
	int resp_code = 200;

	while ((bytes_read = read(cgi->getOutputFd(), buffer, sizeof(buffer))) > 0) {
		cgi_output.append(buffer, bytes_read);
		if (cgi_output.size() > 5) {
			std::string s = cgi_output.substr(2, 3);
			std::stringstream ss(s);
			ss >> resp_code;
			if (resp_code > 201) {
				close(cgi->getOutputFd());
				return (prepareResponse(conn, Response(resp_code)));
			}
		}
	}

	// close cgi script fd
	close(cgi->getOutputFd());

	if (bytes_read == -1) {
		logger.logWithPrefix(Logger::ERROR, "CGI", "Error reading from CGI script");
		return (false);
	}
	std::string resp_body = cgi_output.substr(7);
	//printCGIResponse(resp_body);
	return (prepareResponse(conn, Response(resp_code, resp_body)) > 0);
}

void WebServer::handleCGIOutput(int fd) {
	std::map<int, std::pair<CGI *, Connection *> >::iterator it = _cgi_pool.find(fd);
	if (it == _cgi_pool.end())
		return;

	CGI *cgi = it->second.first;
	Connection *conn = it->second.second;

	_cgi_pool.erase(it);
	epollManage(EPOLL_CTL_DEL, fd, 0);
	prepareCGIResponse(cgi, conn);
	delete cgi;
}

bool WebServer::isCGIFd(int fd) const { return (_cgi_pool.find(fd) != _cgi_pool.end()); }
