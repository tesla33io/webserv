#include "Utils/StringUtils.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include <ctime>
#include <string>

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
	oss << "keep_presistent_connection: " << (keep_persistent_connection ? "true" : "false") << ", ";
	oss << "request_count: " << request_count << ", ";
	oss << "state: " << stateToString(state) << "}";

	return oss.str();
}

//// Response ////

Logger Response::tmplogg_;

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
	setContentLength(body.length());
}

Response::Response(uint16_t code, Connection* conn)
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

Response Response::notFound() {
	Response resp(404);
	resp.setContentType("text/html");
	return resp;
}

Response Response::internalServerError() {
	Response resp(500);
	resp.setContentType("text/html");
	return resp;
}

Response Response::badRequest() {
	Response resp(400);
	resp.setContentType("text/html");
	return resp;
}

Response Response::methodNotAllowed() {
	Response resp(405);
	resp.setContentType("text/html");
	return resp;
}

// OVErLOAD
Response Response::notFound(Connection *conn) {
	Response resp(404, conn);
	resp.setContentType("text/html");
	return resp;
}

Response Response::internalServerError(Connection *conn) {
	Response resp(500, conn);
	resp.setContentType("text/html");
	return resp;
}

Response Response::badRequest(Connection *conn) {
	Response resp(400, conn);
	resp.setContentType("text/html");
	return resp;
}

Response Response::methodNotAllowed(Connection *conn) {
	Response resp(405, conn);
	resp.setContentType("text/html");
	return resp;
}

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
		tmplogg_.logWithPrefix(Logger::DEBUG, "Response", "No custom error page for " + su::to_string(code));
		initFromStatusCode(code);
		return ;
	}
	
	std::ifstream errorFile(conn->getServerConfig()->getErrorPage(code).c_str());
 	if (!errorFile.is_open()) {
		tmplogg_.logWithPrefix(Logger::WARNING, "Response", "Custom error page " + su::to_string(code) + " could not be opened.");
		initFromStatusCode(code);
		return;
	}

	std::ostringstream html;
	html << errorFile.rdbuf();
	body = html.str();
	setContentLength(body.length());
	// HELENE TODO: check for html valid content? - set other values for the response attri?
	setContentType("text/html"); // ?
	errorFile.close();
	tmplogg_.logWithPrefix(Logger::DEBUG, "Response", "Custom error page " + 
		  su::to_string(code) + " has been loaded.");

}

void Response::initFromStatusCode(uint16_t code) {
	reason_phrase = getReasonPhrase(code);
	if (code >= 400) {
		// TODO: check conf for error pages
		// Using built-in error pages
		if (body.empty()) {
			tmplogg_.logWithPrefix(Logger::DEBUG, "Response", "No custom error page for" + su::to_string(code) + " could be used or exist. Generating the default page now.");
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
		}
	}
}
