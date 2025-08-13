/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Structs.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:11:23 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/13 14:51:44 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpServer.hpp"

//// Connection ////

Connection::Connection(int socket_fd)
    : fd(socket_fd),
      keep_persistent_connection(true),
      chunked(false),
      chunk_size(0),
      chunk_bytes_read(0),
      response_ready(false),
      request_count(0),
      state(READING_HEADERS) {
	updateActivity();
}

void Connection::updateActivity() { last_activity = time(NULL); }

bool Connection::isExpired(time_t current_time, int timeout) const {
	return (current_time - last_activity) > timeout;
}

void Connection::resetChunkedState() {
	state = READING_HEADERS;
	chunked = false;
}

std::string Connection::stateToString(Connection::State state) {
	switch (state) {
	case Connection::READING_HEADERS:
		return "READING_HEADERS";
	case Connection::READING_CHUNK_SIZE:
		return "READING_CHUNK_SIZE";
	case Connection::READING_CHUNK_DATA:
		return "READING_CHUNK_DATA";
	case Connection::READING_CHUNK_TRAILER:
		return "READING_CHUNK_TRAILER";
	case Connection::CONTINUE_SENT:
		return "CONTINUE_SENT";
	case Connection::READING_TRAILER:
		return "READING_FINAL_TRAILER";
	case Connection::CHUNK_COMPLETE:
		return "CHUNK_COMPLETE";
	case Connection::REQUEST_COMPLETE:
		return "REQUEST_COMPLETE";
	default:
		return "UNKNOWN_STATE";
	}
}

std::string Connection::toString() {
	std::ostringstream oss;

	oss << "Connection{";
	oss << "clfd: " << fd << ", ";

	char time_buf[26];
	// TODO: do we need thread-safety??
#if defined(_MSC_VER)
	ctime_s(time_buf, sizeof(time_buf), &last_activity);
#else
	ctime_r(&last_activity, time_buf);
#endif
	// ctime_r includes a newline at the end; remove it
	time_buf[24] = '\0';

	oss << "last_activity: " << time_buf << ", ";
	oss << "read_buffer: \"" << read_buffer << "\", ";
	oss << "response_ready: " << (response_ready ? "true" : "false") << ", ";
	oss << "response_status: "
	    << (response_ready ? su::to_string(response.status_code) + " " + response.reason_phrase
	                       : "not ready")
	    << ", ";
	oss << "chunked: " << (chunked ? "true" : "false") << ", ";
	oss << "keep_persistent_connection: " << (keep_persistent_connection ? "true" : "false")
	    << ", ";
	oss << "request_count: " << request_count << ", ";
	oss << "state: " << stateToString(state) << "}";

	return oss.str();
}

//// Response ////

Logger Response::resplogg_("Response", Logger::DEBUG);

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

Response Response::methodNotAllowed(const std::string& allowed) { 
	Response resp(405);
	if (!allowed.empty()) {
		resp.setHeader("Allow", allowed);
	}
	return resp; }

Response Response::internalServerError() { return Response(500); }

Response Response::notImplemented() { return Response(501); }

// OVErLOAD

Response Response::badRequest(Connection *conn) { return Response(400, conn); }

Response Response::forbidden(Connection *conn) { return Response(403, conn); }

Response Response::notFound(Connection *conn) { return Response(404, conn); }

Response Response::methodNotAllowed(Connection *conn, const std::string& allowed) {
	Response resp(405, conn);
	if (!allowed.empty()) {
		resp.setHeader("Allow", allowed);
	}
	return resp; }

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
		return "Found"; // moved temporarily
	case 400:
		return "Bad Request";
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
	case 414:
		return "Request-URI Too Long";
	case 500:
		return "Internal Server Error";
	case 501:
		return "Not Implemented";
	case 503:
		return "Service Unavailable";
	case 505:
		return "HTTP Version Not Supported";
	default:
		return "Unknown Status";
	}
}

void Response::initFromCustomErrorPage(uint16_t code, Connection *conn) {

	reason_phrase = getReasonPhrase(code);

	if (!conn || !conn->getServerConfig() || !conn->getServerConfig()->hasErrorPage(code)) {
		resplogg_.logWithPrefix(Logger::DEBUG, "Response",
		                       "No custom error page for " + su::to_string(code));
		resplogg_.logWithPrefix(Logger::DEBUG, "Response",
		                       "Creating the default error page for " + su::to_string(code));
		initFromStatusCode(code);
		return;
	}
	resplogg_.logWithPrefix(Logger::DEBUG, "Response",
	                       "A custom error page exists for " + su::to_string(code));
	// todo check path again
	std::string fullPath =
	    conn->getServerConfig()->getPrefix() + conn->getServerConfig()->getErrorPage(code);
	std::ifstream errorFile(fullPath.c_str());
	if (!errorFile.is_open()) {
		resplogg_.logWithPrefix(Logger::WARNING, "Response",
		                       "Custom error page " + fullPath + " could not be opened.");
		initFromStatusCode(code);
		return;
	}

	std::ostringstream errorPage;
	errorPage << errorFile.rdbuf();
	body = errorPage.str();
	setContentLength(body.length());
	setContentType(detectContentType(fullPath));
	errorFile.close();
	resplogg_.logWithPrefix(Logger::DEBUG, "Response",
	                       "Custom error page " + su::to_string(code) + " has been loaded.");
}

void Response::initFromStatusCode(uint16_t code) {
	reason_phrase = getReasonPhrase(code);
	if (code >= 400) {
		if (body.empty()) {
			resplogg_.logWithPrefix(Logger::DEBUG, "Response",
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
			resplogg_.logWithPrefix(Logger::DEBUG, "Response",
			                       "Content-Type set to: " + headers["Content-Type"]);
		}
	}
}
