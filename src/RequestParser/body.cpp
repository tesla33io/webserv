/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   body.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 10:46:18 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/01 09:00:08 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"
#include "request_parser.hpp"

bool RequestParsingUtils::check_and_trim_line(std::string &line) {
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

bool RequestParsingUtils::chunked_encoding(std::istringstream &stream, ClientRequest &request) {
	Logger logger;
	std::string line;
	while (std::getline(stream, line)) {
		if (!check_and_trim_line(line))
			return (false);
		std::istringstream chunk_length_stream(line);
		int chunk_length;
		if (!(chunk_length_stream >> chunk_length) || chunk_length < 0) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid chunk length value");
			return (false);
		}
		// Find final chunk
		if (chunk_length == 0) {
			if (!std::getline(stream, line)) {
				logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid chunked encoding");
			}
			if (!check_and_trim_line(line))
				return (false);
			if (!line.empty()) {
				logger.logWithPrefix(Logger::WARNING, "HTTP", "Missing end of chunked body");
				return (false);
			}
			return (true);
		}

		// Parse chunk line
		if (!std::getline(stream, line)) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid chunked encoding");
			return (false);
		}
		std::istringstream chunk_stream(line);
		// Read exactly chunk_length bytes
		std::string chunk(chunk_length, '\0');
		chunk_stream.read(&chunk[0], chunk_length);
		std::streamsize actually_read = chunk_stream.gcount();
		if (actually_read != chunk_length) {
			std::ostringstream msg;
			msg << "Chunk length mismatch: expected " << chunk_length << " bytes, but read "
			    << actually_read << " bytes";
			logger.logWithPrefix(Logger::WARNING, "HTTP", msg.str());
			return (false);
		}
		request.body.append(chunk);
	}
	logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid chunked encoding");
	return (false);
}

bool RequestParsingUtils::parse_body(std::istringstream &stream, ClientRequest &request) {
	Logger logger;
	logger.logWithPrefix(Logger::DEBUG, "HTTP", "Parsing message body");

	const char *content_length_value = find_header(request, "content-length");

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
