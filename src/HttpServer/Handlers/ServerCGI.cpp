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

#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"

void printCGIResponse(const std::string &cgi_output) {
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

bool WebServer::sendCGIResponse(CGI *cgi, Connection *conn) {
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
		return (false);
	}
	printCGIResponse(cgi_output);
	conn->response_ready = true;
	send(conn->fd, cgi_output.c_str(), cgi_output.length(), 0);
	cgi->cleanup();
	delete cgi;
	return (true);
}

ssize_t WebServer::prepareCGIResponse(CGI *cgi, Connection *conn) {
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
		return (false);
	}
	printCGIResponse(cgi_output);
	if (conn->response_ready) {
		_lggr.error(
		    "Trying to prepare a response for a connection that is ready to send another one");
		return (-1);
	}
	conn->cgi_response = cgi_output;
	conn->response_ready = true;
	cgi->cleanup();
	delete cgi;
	conn->response_ready = true;
	return (conn->cgi_response.size());
}

void WebServer::handleCGIOutput(int fd) {
	CGI *cgi;
	Connection *conn;
	for (std::map<int, std::pair<CGI *, Connection *> >::iterator it = _cgi_pool.begin();
	     it != _cgi_pool.end(); ++it) {
		if (fd == it->first) {
			cgi = it->second.first;
			conn = it->second.second;
		}
	}
	sendCGIResponse(cgi, conn);
}

bool WebServer::isCGIFd(int fd) const { return (_cgi_pool.find(fd) != _cgi_pool.end()); }
