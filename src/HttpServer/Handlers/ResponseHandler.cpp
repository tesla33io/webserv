#include "RequestParser/request_parser.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/Utils/StringUtils.hpp"

ssize_t WebServer::sendResponse(const int clfd, const Response &resp) {
	// TODO: some checks if the arguments are fine to work with
	// TODO: make sure that Response has all required headers set up correctly (e.g. Content-Type, Content-Length, etc).
	std::string raw_response = resp.toString();
	return send(clfd, raw_response.c_str(), raw_response.length(), 0);
}

WebServer::Response WebServer::handleGetRequest(const ClientRequest &req) {
	_lggr.debug("Requested path: " + req.uri);
	std::string content = getFileContent(_root_path + req.uri);

	if (content.empty()) {
		_lggr.error("[Resp] File not found: " + req.uri);
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

