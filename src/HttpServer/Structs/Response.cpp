/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:35:52 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/25 19:10:17 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Response.hpp"
#include "src/ConfigParser/Struct.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/Utils/ServerUtils.hpp"

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

Response Response::methodNotAllowed(const std::string &allowed) {
	Response resp(405);
	if (!allowed.empty()) {
		resp.setHeader("Allow", allowed);
	}
	return resp;
}

Response Response::contentTooLarge() { return Response(413); }

Response Response::internalServerError() { return Response(500); }

Response Response::notImplemented() { return Response(501); }

Response Response::badGateway() { return Response(502); }

Response Response::gatewayTimeout() { return Response(504); }

Response Response::HttpNotSupported() { return Response(505); }

// OVErLOAD

Response Response::badRequest(Connection *conn) { return Response(400, conn); }

Response Response::forbidden(Connection *conn) { return Response(403, conn); }

Response Response::notFound(Connection *conn) { return Response(404, conn); }

Response Response::methodNotAllowed(Connection *conn, const std::string &allowed) {
	Response resp(405, conn);
	if (!allowed.empty()) {
		resp.setHeader("Allow", allowed);
	}
	return resp;
}

Response Response::contentTooLarge(Connection *conn) { return Response(413, conn); }

Response Response::internalServerError(Connection *conn) { return Response(500, conn); }

Response Response::notImplemented(Connection *conn) { return Response(501, conn); }

Response Response::badGateway(Connection *conn) { return Response(502, conn); }

Response Response::gatewayTimeout(Connection *conn) { return Response(504, conn); }

Response Response::HttpNotSupported(Connection *conn) { return Response(505, conn); }

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
	case 411:
		return "Length required";
	case 413:
		return "Content Too Large";
	case 414:
		return "URI Too Long";
	case 417:
		return "Expectation Failed";
	case 500:
		return "Internal Server Error";
	case 501:
		return "Not Implemented";
	case 502:
		return "Bad Gateway";
	case 503:
		return "Service Unavailable";
	case 504:
		return "CGI Time-out";
	case 505:
		return "HTTP version not supported";
	default:
		return "Unknown Status";
	}
}

void Response::initFromCustomErrorPage(uint16_t code, Connection *conn) {

	reason_phrase = getReasonPhrase(code);

	if (!conn || !conn->getServerConfig() || !conn->getServerConfig()->hasErrorPage(code)) {
		initFromStatusCode(code);
		return;
	}

	std::string fullPath = conn->getServerConfig()->getErrorPage(code);
	std::ifstream errorFile(fullPath.c_str());
	if (!errorFile.is_open()) {
		initFromStatusCode(code);
		return;
	}

	std::ostringstream errorPage;
	errorPage << errorFile.rdbuf();
	body = errorPage.str();
	setContentLength(body.length());
	setContentType(detectContentType(fullPath));
	errorFile.close();
}

void Response::initFromStatusCode(uint16_t code) {
	reason_phrase = getReasonPhrase(code);
	if (code >= 400) {
		if (body.empty()) {
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
		}
	}
}
