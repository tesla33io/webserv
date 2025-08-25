/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Headers.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 10:46:05 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/23 22:52:46 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestParser.hpp"

/* Checks */
uint16_t RequestParsingUtils::checkHeader(std::string &name, std::string &value,
                                      ClientRequest &request, Logger &logger) {

	std::string l_name = su::to_lower(name);
	std::string l_value = su::to_lower(value);
	
	// Check header size
	if (name.size() > MAX_HEADER_NAME_LENGTH) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Header name too big");
		return 400;
	} else if (value.size() > MAX_HEADER_VALUE_LENGTH) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Header value too big");
		return 400;
	}
	// Check for duplicate header
	if (findHeader(request, l_name, logger)) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Duplicate header present");
		return 400;
	}
	// Chunk encoding + content length validation
	if (l_name == "transfer-encoding") {
		if (l_value == "chunked") {
			request.chunked_encoding = true;
		} else {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid transfer encoding");
			return 400;
		}
	}
	// content length
	if (l_name == "content-length") {
		std::istringstream iss(value);
		ssize_t parsed_length = -1;
		iss >> parsed_length;
		if (!iss || !iss.eof() || parsed_length < 0) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid Content-Length: " + value);
			return 400;
		}
		// stored 
		request.content_length = parsed_length; 
	}
	// expect
	if (l_name == "expect") {
		if (l_value == "100-continue") {
			request.expect_continue = true;
			logger.debug("Expect header: " + l_value);
		} else {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Unsupported Expect header: " + value);
			return 417; // expectation failed
		}
}
	return 0;
}

/* Parser */
uint16_t RequestParsingUtils::parseHeaders(std::istringstream &stream, ClientRequest &request, Logger &logger) {
	std::string line;
	logger.logWithPrefix(Logger::DEBUG, "HTTP", "Parsing headers");
	int header_count = 0;
	while (std::getline(stream, line)) {
		// Check header count limit
		if (++header_count > 100) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Too many headers");
			return 400;
		}

		uint16_t trim_error = checkNTrimLine(line, logger);
		if (trim_error != 0)
			return trim_error;

		if (line.empty()) {
			// Check for host header
			if (!findHeader(request, "host", logger)) {
				logger.logWithPrefix(Logger::WARNING, "HTTP", "No Host header present");
				return 400;
			}
			// Check for Transfer-encoding=chunked and Content-length headers
			if (request.chunked_encoding && findHeader(request, "content-length", logger)) {
				logger.logWithPrefix(Logger::WARNING, "HTTP",
				                     "Content-length header present with chunked encoding");
				return 400;
			}
			return 0;
		}

		size_t colon = line.find(':');
		if (colon == std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid header format");
			return 400;
		}
		std::string name = trimSide(line.substr(0, colon), 1);
		std::string value = trimSide(line.substr(colon + 1), 3);

		if (name.empty()) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Empty header name");
			return 400;
		}
		// Check for spaces in header name (invalid according to HTTP spec)
		if (name.find(' ') != std::string::npos || name.find('\t') != std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid header name (contains spaces)");
			return 400;
		}
		// Check for valid header name characters
		for (size_t i = 0; i < name.length(); ++i) {
			char c = name[i];
			if (!std::isalnum(c) && c != '-' && c != '_') {
				logger.logWithPrefix(Logger::WARNING, "HTTP",
				                     "Invalid character in header name: " + name);
				return 400;
			}
		}
		uint16_t header_error = checkHeader(name, value, request, logger);
		if (header_error != 0)
			return header_error;
		request.headers[su::to_lower(name)] = value;
	}
	if (request.content_length == -1 && request.chunked_encoding == false) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "No content length, no chunk");
		return 411;
	}
	logger.logWithPrefix(Logger::WARNING, "HTTP", "Missing final CRLF");
	return 400;
}
