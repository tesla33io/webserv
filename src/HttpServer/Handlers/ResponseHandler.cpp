#include "ConfigParser/config_parser.hpp"
#include "RequestParser/request_parser.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/Utils/StringUtils.hpp"
#include <string>
#include <vector>

ssize_t WebServer::sendResponse(const int clfd, const Response &resp) {
	// TODO: some checks if the arguments are fine to work with
	// TODO: make sure that Response has all required headers set up correctly (e.g. Content-Type,
	// Content-Length, etc).
	_lggr.debug("Sending a response [" + su::to_string(resp.status_code) + "] back to fd " +
	            su::to_string(clfd));
	std::string raw_response = resp.toString();
	return send(clfd, raw_response.c_str(), raw_response.length(), 0);
}

WebServer::Response WebServer::handleGetRequest(ClientRequest &req) {
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

