#include "ConfigParser/config_parser.hpp"
#include "RequestParser/request_parser.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/Utils/StringUtils.hpp"
#include <cstdio>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <vector>


ssize_t WebServer::prepareResponse(Connection *conn, const Response &resp) {
	// TODO: some checks if the arguments are fine to work with
	// TODO: make sure that Response has all required headers set up correctly (e.g. Content-Type,
	// Content-Length, etc).
	if (conn->response_ready) {
		_lggr.error(
			"Trying to prepare a response for a connection that is ready to sent another one");
		_lggr.error("Current response: " + conn->response.toShortString());
		_lggr.error("Trying to prepare response: " + resp.toShortString());
		return -1;
	}
	_lggr.debug("Saving a response [" + su::to_string(resp.status_code) + "] for fd " +
				su::to_string(conn->fd));
	conn->response = resp;
	conn->response_ready = true;
	return conn->response.toString().size();
	// return send(clfd, raw_response.c_str(), raw_response.length(), 0);
}


bool WebServer::sendResponse(Connection *conn) {
	if (!conn->response_ready) {
		_lggr.error("Response is not ready to be sent back to the client");
		_lggr.debug("Error for clinet " + conn->toString());
		return false;
	}
	_lggr.debug("Sending response [" + conn->response.toShortString() +
				"] back to fd: " + su::to_string(conn->fd));
	std::string raw_response = conn->response.toString();
	epollManage(EPOLL_CTL_MOD, conn->fd, EPOLLIN);
	conn->response.reset();
	conn->response_ready = false;
	return send(conn->fd, raw_response.c_str(), raw_response.size(), MSG_NOSIGNAL) != -1;
}


Response WebServer::handleGetRequest(ClientRequest &req) {
	_lggr.debug("Requested path: " + req.uri);
	
	Connection* conn = getConnection(req.clfd);
	if (!conn || !conn->servConfig) {
		_lggr.error("[Resp] Connection instance not found for client fd : " + su::to_string(req.clfd));
		return Response::internalServerError();
	}
	
	// initialize the correct locConfig // defualt "/"
	LocConfig *match = findBestMatch(req.uri, conn->servConfig->locations);
	conn->locConfig = match; // Set location context
	
	if (!match) {
		_lggr.error("[Resp] No matched location for : " + req.uri);
		return Response::internalServerError(conn);
	}

	// GET method allowed in the loc? 
	if (!allowedMethod(req, conn)) {
		_lggr.debug("GET method not allowed for location: " + match->path);
		return Response::methodNotAllowed(conn);
	}
	
	// Build file path
	std::string fullPath = buildFullPath(req.uri, match);

	// does it exist as a DIRECTORY? stat
	if (isDirectoryRequest(fullPath)) {
		// TODO : is it possible to have a directory request wo the ending '/'?
		if (!isDirectory(fullPath.c_str())) {
			_lggr.error("Path not found: " + fullPath);
			return Response::notFound(conn);
		}
		else // directory match found 
			return handleDirectoryRequest(conn, fullPath);
	}

	else { // no directory request -> file request
		if (!isRegularFile(fullPath.c_str())) {
			_lggr.error("Path not found: " + fullPath);
			return Response::notFound(conn);
		}
		else // directory match found 
			return handleFileRequest(conn, fullPath);
	}

	_lggr.error("Path not found: " + fullPath);
	return Response::notFound(conn);
}


// Serving the index file or listing if possible
Response WebServer::handleDirectoryRequest(Connection* conn, const std::string& dir_path) {
	_lggr.debug("Handling directory request: " + dir_path);
	
	// Try to serve index file 
	if (!conn->locConfig->index.empty()) {
		std::string index_path = dir_path + conn->locConfig->index;
		_lggr.debug("Trying index file: " + index_path);
		if (isRegularFile(index_path.c_str())) {
			_lggr.debug("Found index file, serving: " + index_path);
			return handleFileRequest(conn, index_path);
		}
	}
	
	// TODO : Handle autoindex 
	// if (conn->locConfig->autoindex) {
	// 	_lggr.debug("Autoindex on, generating directory listing");
	// 	return generateDirectoryListing(conn, dir_path);
	// }
	
	// No index file and no autoindex
	_lggr.debug("No index file, autoindex disabled");
	return Response::notFound(conn);
}



// serving the file if found
Response WebServer::handleFileRequest(Connection* conn, const std::string& file_path) {
	_lggr.debug("Handling file request: " + file_path);
	// Read file content
	std::string content = getFileContent(file_path);
	if (content.empty()) {
		_lggr.error("Failed to read file: " + file_path);
		return Response::internalServerError(conn);
	}
	// Create response
	Response resp(200, content);
	// resp.setContentType(); TODO : other attributes needed for response?
	_lggr.debug("Successfully serving file: " + file_path + 
				" (" + su::to_string(content.length()) + " bytes)");
	return resp;
}


bool WebServer::allowedMethod(const ClientRequest& req, Connection* conn) {

	if (!conn->locConfig->hasMethod(req.method)) {
		_lggr.debug("Method " + req.method + "is not allowed for location " + 
				conn->locConfig->path);
	}
	return true;
}


std::string WebServer::detectContentType(const std::string &path) {
	std::string ft_html = ".html";
	if (path.length() > ft_html.length() &&
		std::equal(ft_html.rbegin(), ft_html.rend(), path.rbegin())) {
		return "text/html";
	} else {
		return "text/plain";
	}
}
