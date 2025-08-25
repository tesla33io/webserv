/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:41:32 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/25 14:21:39 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/HttpServer.hpp"

Connection::Connection(int socket_fd)
    : fd(socket_fd),
      keep_persistent_connection(true),
      body_bytes_read(0),
      content_length(-1),
      chunked(false),
      chunk_size(0),
      chunk_bytes_read(0),
	  cgi_response(""),
      response_ready(false),
      request_count(0),
      should_close(0),
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
	oss << "keep_presistent_connection: " << (keep_persistent_connection ? "true" : "false")
	    << ", ";
	oss << "request_count: " << request_count << ", ";
	oss << "state: " << stateToString(state) << "}";

	return oss.str();
}
