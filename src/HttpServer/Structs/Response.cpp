/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:35:52 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 13:43:33 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Response.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/ConfigParser/ConfigParser.hpp"
#include "src/HttpServer/HttpServer.hpp"

// Local helpers for MIME detection to avoid depending on WebServer
static std::string getExtensionForMime(const std::string &path) {
	std::size_t dot_pos = path.find_last_of('.');
	std::size_t qm_pos = path.find_first_of('?');
	if (qm_pos != std::string::npos && dot_pos < qm_pos)
		return path.substr(dot_pos, qm_pos - dot_pos);
	else if (qm_pos == std::string::npos && dot_pos != std::string::npos)
		return path.substr(dot_pos);
	return "";
}

static std::string detectContentTypeLocal(const std::string &path) {
	std::map<std::string, std::string> cTypes;
	cTypes[".css"] = "text/css";
	cTypes[".js"] = "application/javascript";
	cTypes[".html"] = "text/html";
	cTypes[".htm"] = "text/html";
	cTypes[".json"] = "application/json";
	cTypes[".png"] = "image/png";
	cTypes[".jpg"] = "image/jpeg";
	cTypes[".jpeg"] = "image/jpeg";
	cTypes[".gif"] = "image/gif";
	cTypes[".svg"] = "image/svg+xml";
	cTypes[".ico"] = "image/x-icon";
	cTypes[".txt"] = "text/plain";
	cTypes[".pdf"] = "application/pdf";
	cTypes[".zip"] = "application/zip";

	std::string ext = getExtensionForMime(path);
	std::map<std::string, std::string>::const_iterator it = cTypes.find(ext);
	if (it != cTypes.end())
		return it->second;
	return "application/octet-stream";
}

Logger Response::tmplogg_("Response", Logger::DEBUG);

Response::Response()
    : version("HTTP/1.1"),
      status_code(0),
      reason_phrase("Not Ready") {}

Response::Response(uint16_t code)
    : version("HTTP/1.1"),
      status_code(code) {
	initFromStatusCode(code);
}

Response::Response(uint16_t code, const std::string &response_body)
    : version("HTTP/1.1"),
      status_code(code),
      body(response_body) {
	initFromStatusCode(code);
}

Response::Response(uint16_t code, Connection *conn)
    : version("HTTP/1.1"),
      status_code(code) {
	initFromCustomErrorPage(code, conn);
}

std::string Response::toString() const {
	std::ostringstream response_stream;
	response_stream << version << " " << status_code << " " << reason_phrase << "\r\n";
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
	     it != headers.end(); ++it) {
		response_stream << it->first << ": " << it->second << "\r\n";
	}
	response_stream << "\r\n";
	response_stream << body;
	return response_stream.str();
}

std::string Response::toStringHeadersOnly() const {
	std::ostringstream response_stream;
	response_stream << version << " " << status_code << " " << reason_phrase << "\r\n";
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
	     it != headers.end(); ++it) {
		response_stream << it->first << ": " << it->second << "\r\n";
	}
	response_stream << "\r\n";
	return response_stream.str();
}

std::string Response::toShortString() const {
	std::ostringstream response_stream;
	response_stream << version << " " << status_code << " " << reason_phrase;
	if (headers.find("Content-Length") != headers.end()) {
		response_stream << " Content-Len.: " << headers.find("Content-Length")->second;
	}
	return response_stream.str();
}

void Response::reset() {
	status_code = 0;
	reason_phrase = "Not ready";
	headers.clear();
	body.clear();
}

Response Response::continue_() { return Response(100); }

Response Response::ok(const std::string &body) { return Response(200, body); }

Response Response::badRequest() { return Response(400); }

Response Response::forbidden() { return Response(403); }

Response Response::notFound() { return Response(404); }

Response Response::methodNotAllowed() { return Response(405); }

Response Response::internalServerError() { return Response(500); }

Response Response::notImplemented() { return Response(501); }

// OVErLOAD

Response Response::badRequest(Connection *conn) { return Response(400, conn); }

Response Response::forbidden(Connection *conn) { return Response(403, conn); }

Response Response::notFound(Connection *conn) { return Response(404, conn); }

Response Response::methodNotAllowed(Connection *conn) { return Response(405, conn); }

Response Response::internalServerError(Connection *conn) { return Response(500, conn); }

Response Response::notImplemented(Connection *conn) { return Response(501, conn); }

std::string Response::getReasonPhrase(uint16_t code) const {
	switch (code) {
	case 100:
		return "Continue";
	case 200:
		return "OK";
	case 201:
		return "Created";
	case 204:
		return "No Content";
	case 301:
		return "Moved Permanently";
	case 302:
		return "Found";
	case 304:
		return "Not Modified";
	case 400:
		return "Bad Request";
	case 401:
		return "Unauthorized";
	case 403:
		return "Forbidden";
	case 404:
		return "Not Found";
	case 405:
		return "Method Not Allowed";
	case 408:
		return "Request Timeout";
	case 413:
		return "Content Too Large";
	case 500:
		return "Internal Server Error";
	case 501:
		return "Not Implemented";
	case 502:
		return "Bad Gateway";
	case 503:
		return "Service Unavailable";
	default:
		return "Unknown Status";
	}
}

void Response::initFromCustomErrorPage(uint16_t code, Connection *conn) {

	reason_phrase = getReasonPhrase(code);

	if (!conn || !conn->getServerConfig() || !conn->getServerConfig()->hasErrorPage(code)) {
		tmplogg_.logWithPrefix(Logger::DEBUG, "Response",
		                       "No custom error page for " + su::to_string(code));
		tmplogg_.logWithPrefix(Logger::DEBUG, "Response",
		                       "Creating the default error page for " + su::to_string(code));
		initFromStatusCode(code);
		return;
	}
	tmplogg_.logWithPrefix(Logger::DEBUG, "Response",
	                       "A custom error page exists for " + su::to_string(code));
	// todo check path again
	std::string fullPath =
	    conn->getServerConfig()->getRootPrefix() + conn->getServerConfig()->getErrorPage(code);
	std::ifstream errorFile(fullPath.c_str());
	if (!errorFile.is_open()) {
		tmplogg_.logWithPrefix(Logger::WARNING, "Response",
		                       "Custom error page " + fullPath + " could not be opened.");
		initFromStatusCode(code);
		return;
	}

	std::ostringstream errorPage;
	errorPage << errorFile.rdbuf();
	body = errorPage.str();
	setContentLength(body.length());
	setContentType(detectContentTypeLocal(fullPath));
	errorFile.close();
	tmplogg_.logWithPrefix(Logger::DEBUG, "Response",
	                       "Custom error page " + su::to_string(code) + " has been loaded.");
}

void Response::initFromStatusCode(uint16_t code) {
	reason_phrase = getReasonPhrase(code);
	if (code >= 400) {
		if (body.empty()) {
			tmplogg_.logWithPrefix(Logger::DEBUG, "Response",
			                       "No custom error page for " + su::to_string(code) +
			                           " could be used or exist. Generating the default page now.");
			std::ostringstream html;
			html << "<!DOCTYPE html>\n"
			     << "<html>\n"
			     << "<head>\n"
			     << "<title>" << code << " DX</title>\n"
			     << "<style>\n"
			     << "@import "
			        "url('https://fonts.googleapis.com/"
			        "css2?family=Space+Mono:ital,wght@0,400;0,700;1,400;1,700&display=swap'"
			        ");\n"
			     << "body { font-family: \"Space Mono\", monospace; text-align: center; "
			        "background-color: "
			        "#f8f9fa; "
			        "margin: 0; padding: 0; }\n"
			     << "h1 { color: #ff5555; margin-top: 50px; font-weight: 700; font-style: "
			        "normal; }\n"
			     << "p { color: #6c757d; font-size: 18px; }"
			     << "footer { color: #dcdcdc; position: "
			        "fixed; width: 100%; margin-top: 50px; }\n"
			     << "</style>\n"
			     << "</head>\n"
			     << "<body>\n"
			     << "<h1>Error " << code << ": " << reason_phrase << "</h1>\n"
			     << "<p>The server encountered an issue and could not complete your "
			        "request.</p>\n"
			     << "<a href=\"https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Status/"
			     << code << "\" target=\"_blank\" rel=\"noopener noreferrer\">MDN Web Docs - "
			     << code << "</a>"
			     << "<footer>" << __WEBSERV_VERSION__ << "</footer>"
			     << "</body>\n"
			     << "</html>\n";
			body = html.str();
			setContentLength(body.length());
			setContentType("text/html");
			tmplogg_.logWithPrefix(Logger::DEBUG, "Response",
			                       "Content-Type set to: " + headers["Content-Type"]);
		}
	}
}
