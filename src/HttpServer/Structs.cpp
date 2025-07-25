#include "src/HttpServer/HttpServer.hpp"
#include <ctime>
#include <string>

//// ConnectionInfo ////

WebServer::ConnectionInfo::ConnectionInfo(int socket_fd)
    : clfd(socket_fd),
      chunked(false),
      keep_alive(false),
      request_count(0),
      state(READING_HEADERS) {
	updateActivity();
}

void WebServer::ConnectionInfo::updateActivity() { last_activity = time(NULL); }

bool WebServer::ConnectionInfo::isExpired(time_t current_time, int timeout) const {
	return (current_time - last_activity) > timeout;
}

void WebServer::ConnectionInfo::resetChunkedState() {
	state = READING_HEADERS;
	chunked = false;
}

std::string stateToString(WebServer::ConnectionInfo::State state) {
	switch (state) {
	case WebServer::ConnectionInfo::READING_HEADERS:
		return "READING_HEADERS";
	case WebServer::ConnectionInfo::READING_CHUNK_SIZE:
		return "READING_CHUNK_SIZE";
	case WebServer::ConnectionInfo::READING_CHUNK_DATA:
		return "READING_CHUNK_DATA";
	case WebServer::ConnectionInfo::READING_CHUNK_TRAILER:
		return "READING_CHUNK_TRAILER";
	case WebServer::ConnectionInfo::READING_TRAILER:
		return "READING_FINAL_TRAILER";
	case WebServer::ConnectionInfo::CHUNK_COMPLETE:
		return "CHUNK_COMPLETE";
	default:
		return "UNKNOWN_STATE";
	}
}

std::string WebServer::ConnectionInfo::toString() {
	std::ostringstream oss;

	oss << "Connection{";
	oss << "clfd: " << clfd << ", ";

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
	oss << "buffer: \"" << buffer << "\", ";
	oss << "chunked: " << (chunked ? "true" : "false") << ", ";
	oss << "keep_alive: " << (keep_alive ? "true" : "false") << ", ";
	oss << "request_count: " << request_count << ", ";
	oss << "chunk_state: " << stateToString(state) << "}";

	return oss.str();
}

//// Response ////

WebServer::Response::Response()
    : version("HTTP/1.1"),
      status_code(200),
      reason_phrase("OK") {}
WebServer::Response::Response(uint16_t code)
    : version("HTTP/1.1"),
      status_code(code) {
	initFromStatusCode(code);
}

WebServer::Response::Response(uint16_t code, const std::string &response_body)
    : version("HTTP/1.1"),
      status_code(code),
      body(response_body) {
	initFromStatusCode(code);
	setContentLength(body.length());
}

void WebServer::Response::setStatus(uint16_t code) {
	status_code = code;
	reason_phrase = getReasonPhrase(code);
}

void WebServer::Response::setHeader(const std::string &name, const std::string &value) {
	headers[name] = value;
}

void WebServer::Response::setContentType(const std::string &ctype) {
	headers["Content-Type"] = ctype;
}

void WebServer::Response::setContentLength(size_t length) {
	headers["Content-Length"] = su::to_string(length);
}

std::string WebServer::Response::toString() const {
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

WebServer::Response WebServer::Response::continue_() { return Response(100); }

WebServer::Response WebServer::Response::ok(const std::string &body) { return Response(200, body); }

WebServer::Response WebServer::Response::notFound() {
	Response resp(404);
	resp.setContentType("text/html");
	return resp;
}

WebServer::Response WebServer::Response::internalServerError() {
	Response resp(500);
	resp.setContentType("text/html");
	return resp;
}

WebServer::Response WebServer::Response::badRequest() {
	Response resp(400);
	resp.setContentType("text/html");
	return resp;
}

WebServer::Response WebServer::Response::methodNotAllowed() {
	Response resp(405);
	resp.setContentType("text/html");
	return resp;
}

std::string WebServer::Response::getReasonPhrase(uint16_t code) const {
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
void WebServer::Response::initFromStatusCode(uint16_t code) {
	reason_phrase = getReasonPhrase(code);
	if (code >= 400) {
		// TODO: check conf for error pages
		// Using built-in error pages
		if (body.empty()) {
			std::ostringstream html;
			html << "<!DOCTYPE html>\n"
			     << "<html>\n"
			     << "<head>\n"
			     << "<title>Error | " << code << "</title>\n"
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
			     << "p { color: #6c757d; font-size: 18px; }\n"
			     << "</style>\n"
			     << "</head>\n"
			     << "<body>\n"
			     << "<h1>Error " << code << ": " << reason_phrase << "</h1>\n"
			     << "<p>The server encountered an issue and could not complete your "
			        "request.</p>\n"
			     << "<a href=\"https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Status/"
			     << code << "\" target=\"_blank\" rel=\"noopener noreferrer\">MDN Web Docs - "
			     << code << "</a>"
			     << "</body>\n"
			     << "</html>\n";
			body = html.str();
			setContentLength(body.length());
		}
	}
}
