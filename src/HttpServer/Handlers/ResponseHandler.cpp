/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ResponseHandler.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:08:41 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/25 14:59:39 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/HttpServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/Structs/WebServer.hpp"

ssize_t WebServer::prepareResponse(Connection *conn, const Response &resp) {
	if (conn->response_ready) {
		_lggr.error(
		    "Trying to prepare a response for a connection that is ready to send another one");
		_lggr.error("Current response: " + conn->response.toShortString());
		_lggr.error("Trying to prepare response: " + resp.toShortString());
		return -1;
	}
	_lggr.debug("Saving a response [" + su::to_string(resp.status_code) + "] for fd " +
	            su::to_string(conn->fd));
	_lggr.debug("Response :" + resp.toShortString());
	conn->response = resp;
	conn->response_ready = true;
	return conn->response.toString().size();
}

bool WebServer::sendResponse(Connection *conn) {
	std::string raw_response;

	_lggr.debug("Current state of response [" + conn->response.toShortString());
	_lggr.debug("Sending response [" + conn->response.toShortString() +
	            "] back to fd: " + su::to_string(conn->fd));
	std::cout << conn->response.toShortString() << "] back to fd: " << su::to_string(conn->fd) << std::endl;
	
	if (conn->cgi_response != "") {
		raw_response = conn->cgi_response;
		conn->cgi_response = "";
	} else {
		raw_response = conn->response.toString();
		conn->response.reset();
	}
	bool send_success = send(conn->fd, raw_response.c_str(), raw_response.size(), MSG_NOSIGNAL) != -1;
	if (!send_success) {
		return false;
	}

	epollManage(EPOLL_CTL_MOD, conn->fd, EPOLLIN);
	conn->response_ready = false;
	conn->state = Connection::READING_HEADERS;
	return true;
}

// Serving the index file or listing if possible
Response WebServer::respDirectoryRequest(Connection *conn, const std::string &fullDirPath) {
	_lggr.debug("Handling directory request: " + fullDirPath);

	// Try to serve index file
	if (!conn->locConfig->index.empty()) {
		std::string fullIndexPath = fullDirPath + conn->locConfig->index;
		_lggr.debug("Trying index file: " + fullIndexPath);
		if (checkFileType(fullIndexPath.c_str()) == ISREG) {
			_lggr.debug("Found index file, serving: " + fullIndexPath);
			return respFileRequest(conn, fullIndexPath);
		}
	}

	// Handle autoindex
	if (conn->locConfig->autoindex) {
		_lggr.debug("Autoindex on, generating directory listing");
		return generateDirectoryListing(conn, fullDirPath);
	}

	// No index file and no autoindex
	_lggr.debug("No index file, autoindex disabled");
	return Response::notFound(conn);
}

// serving the file if found
Response WebServer::respFileRequest(Connection *conn, const std::string &fullFilePath) {
	_lggr.debug("Handling file request: " + fullFilePath);
	// Read file content
	std::string content = getFileContent(fullFilePath);
	// this check is redondant as it has already been checked
	if (content.empty()) {
		_lggr.error("Failed to read file: " + fullFilePath);
		return Response::notFound(conn);
	}
	// Create response
	Response resp(200, content);
	resp.setContentType(detectContentType(fullFilePath));
	resp.setContentLength(content.length());
	_lggr.debug("Successfully serving file: " + fullFilePath + " (" +
	            su::to_string(content.length()) + " bytes)");
	return resp;
}

Response WebServer::respReturnDirective(Connection *conn, uint16_t code, std::string target) {
	_lggr.debug("Handling return directive '" + su::to_string(code) + "' to " + target);

	if (code > 399)
		return Response(code, conn);

	Response resp(code);
	resp.setHeader("Location", target);
	std::ostringstream html;
	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head>\n"
	     << "<title>Redirecting...</title>\n"
	     << "</head>\n"
	     << "<body>\n"
	     << "<h1>Redirecting</h1>\n"
	     << "<p>The document has moved <a href=\"#\">here</a>.</p>\n"
	     << "</body>\n"
	     << "</html>\n";
	resp.body = html.str();
	resp.setContentType("text/html");
	resp.setContentLength(resp.body.length());
	_lggr.debug("Generated redirect response");

	return resp;
}
