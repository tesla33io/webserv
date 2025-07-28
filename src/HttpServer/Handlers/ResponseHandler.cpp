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
	// TODO: error checks for HOST header (what if `Host:`?)
	std::string host_header = req.headers["host"];
	std::string host = host_header.substr(0, host_header.find(":"));
	_lggr.debug("Host: " + host);
	std::string port = host_header.substr(host.length() + 1, std::string::npos);
	_lggr.debug("Port: " + port);

	ServerConfig *req_host = NULL;
	for (std::vector<ServerConfig>::iterator it = _confs.begin(); it != _confs.end(); ++it) {
		// TODO: handle check for servers with 0.0.0.0 host (should ignore host in this case?)
		if (((it->host != "0.0.0.0" && it->host == host) || it->host == "0.0.0.0") &&
		    it->port == std::atoi(port.c_str())) {
			req_host = &(*it);
		}
	}
	if (!req_host) {
		_lggr.error("Did not found server from the list to server the request");
		return Response::internalServerError();
	}
	_lggr.debug("Found responsible server: " + req_host->host + " [" +
	            su::to_string(req_host->server_fd) + "]");

	for (std::vector<LocConfig>::iterator it = req_host->locations.begin();
	     it != req_host->locations.end(); ++it) {
		_lggr.debug("Location: " + it->path);
	}

	LocConfig *match = findBestMatch(req.uri, req_host->locations);
	// TODO: relative path, e.g.: `./bla`
	_lggr.debug("Found best match with " + match->path);
	if (!match) {
		// TODO: handle error when no location was matched
	}

	bool isRelative = match->root[0] == '.';
	if (isRelative) {
		match->root = match->root.substr(1); // skip first .
	}

	std::string full_uri = _root_prefix_path + match->root + req.uri;
	if (req.uri == match->path && isDirectory(full_uri.c_str())) {
		// Use index directive
		full_uri += match->index[0]; // TODO: figure out whta to do when there are many index's
		                             // TODO: check if it's dir
	}

	std::string content = getFileContent(full_uri);

	if (content.empty()) {
		_lggr.error("[Resp] File not found: " + _root_prefix_path + match->root + req.uri);
		return Response::notFound();
	} else {
		// TODO: Implement proper content-type detection
		return Response(200, content);
	}
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

