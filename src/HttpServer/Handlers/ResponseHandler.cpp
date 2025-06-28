#include "src/HttpServer/HttpServer.hpp"
#include "src/Utils/StringUtils.hpp"
#include <algorithm>

std::string WebServer::handleGetRequest(const std::string &path) {
	_lggr.debug("Requested path: " + path);
	std::string content = getFileContent(path);

	if (content.empty()) {
		_lggr.error("[Resp] File not found: " + path);
		return generateErrorResponse(404);
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

std::string WebServer::generateErrorResponse(int errorCode) {
	// Static map of error codes to messages
	static std::map<int, std::string> errorMessages;

	// Populate the map only once
	if (errorMessages.empty()) {
		errorMessages[400] = "Bad Request";
		errorMessages[401] = "Unauthorized";
		errorMessages[403] = "Forbidden";
		errorMessages[404] = "Not Found";
		errorMessages[405] = "Method Not Allowed";
		errorMessages[408] = "Request Timeout";
		errorMessages[413] = "Content Too Large";
		errorMessages[500] = "Internal Server Error";
		errorMessages[501] = "Not Implemented";
		errorMessages[502] = "Bad Gateway";
		errorMessages[503] = "Service Unavailable";
	}

	// Find the error message for the given error code
	std::string message;
	std::map<int, std::string>::const_iterator it = errorMessages.find(errorCode);
	if (it != errorMessages.end()) {
		message = it->second;
	} else {
		message = "Unknown Error";
	}

	// Create the HTML content
	std::ostringstream html;
	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head>\n"
	     << "<title>Error " << errorCode << "</title>\n"
	     << "<style>\n"
	     << "body { font-family: Arial, sans-serif; text-align: center; background-color: #f8f9fa; "
	        "margin: 0; padding: 0; }\n"
	     << "h1 { color: #dc3545; margin-top: 50px; }\n"
	     << "p { color: #6c757d; font-size: 18px; }\n"
	     << "</style>\n"
	     << "</head>\n"
	     << "<body>\n"
	     << "<h1>Error " << errorCode << ": " << message << "</h1>\n"
	     << "<p>The server encountered an issue and could not complete your request.</p>\n"
	     << "</body>\n"
	     << "</html>\n";

	// Create the full HTTP response
	std::ostringstream response;
	response << "HTTP/1.1 " << errorCode << " " << message << "\r\n"
	         << "Content-Type: text/html\r\n"
	         << "Content-Length: " << html.str().length() << "\r\n"
	         << "Connection: close\r\n\r\n"
	         << html.str();

	return response.str();
}
