/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Body.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 10:46:18 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:18:09 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestParser.hpp"

bool RequestParsingUtils::checkNTrimLine(std::string &line) {
	Logger logger;
	// Check line ending (\r\n)
	if (line.empty()) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid line ending");
		return (false);
	}
	if (!line.empty() && line[line.length() - 1] != '\r') {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid line ending");
		return (false);
	}
	if (!line.empty() && line[line.length() - 1] == '\r')
		line.erase(line.length() - 1);
	return (true);
}

bool RequestParsingUtils::parseBody(std::istringstream &stream, ClientRequest &request) {
	Logger logger;
	logger.logWithPrefix(Logger::DEBUG, "HTTP", "Parsing message body");

	const char *content_length_value = findHeader(request, "content-length");

	// Enforce Content-Length for POST even if body is empty
	if (!content_length_value) {
		if (request.method == "POST") {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Missing Content-Length for POST");
			return (false);
		}

		// For GET/DELETE: accept if no body follows
		std::streampos start = stream.tellg();
		char probe;
		if (stream.get(probe)) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Request has body but no Content-Length");
			return (false);
		}
		// Rewind and accept
		stream.clear();
		stream.seekg(start);
		return (true);
	}

	// Validate and parse Content-Length
	std::istringstream content_length_stream(content_length_value);
	int content_length;
	if (!(content_length_stream >> content_length) || content_length < 0) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid Content-Length");
		return (false);
	}
	// Read exactly content_length bytes
	std::string body(content_length, '\0');
	stream.read(&body[0], content_length);
	std::streamsize actually_read = stream.gcount();
	if (actually_read != content_length) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Body length mismatch: expected " + su::to_string(content_length) +
		                         " bytes, but read " + su::to_string(actually_read) + " bytes");
		return (false);
	}
	request.body = body;
	return (true);
}
