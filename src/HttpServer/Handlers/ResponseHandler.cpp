#include "src/HttpServer/HttpServer.hpp"
#include "src/Utils/StringUtils.hpp"
#include <algorithm>

std::string WebServer::handleGetRequest(const std::string &path) {
	_lggr.debug("Requested path: " + path);
	std::string content = getFileContent(path);

	if (content.empty()) {
		_lggr.error("[Resp] File not found: " + path);
		return "HTTP/1.1 404 Not Found\r\n"
		       "Content-Type: text/plain\r\n"
		       "Content-Length: 13\r\n\r\n"
		       "404 Not Found";
	} else {
		// TODO: Implement proper content-type detection
		return "HTTP/1.1 200 OK\r\n"
		       "Content-Type: " +
		       detectContentType(path) +
		       "\r\n"
		       "Content-Length: " +
		       string_utils::to_string<int>(content.size()) + "\r\n\r\n" + content;
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

