/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Headers.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 10:46:05 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/25 14:23:50 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestParser.hpp"

/* Checks */
uint16_t RequestParsingUtils::checkHeader(std::string &name, std::string &value,
                                          ClientRequest &request, Logger &logger) {
	// CRLF or null byte injection prevention in header name and value
	if (name.find('\r') != std::string::npos || name.find('\n') != std::string::npos || name.find('\0') != std::string::npos ||
	    value.find('\r') != std::string::npos || value.find('\n') != std::string::npos || value.find('\0') != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Header injection attempt detected");
		return 400;
	}

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

	// Non-ASCII characters check (allow tab '\t')
	for (size_t i = 0; i < value.length(); ++i) {
		unsigned char ch = static_cast<unsigned char>(value[i]);
		if (ch > 0x7E && ch != '\t') {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Non-ASCII character in header value");
			return 400;
		}
	}

	// Transfer-Encoding
	if (l_name == "transfer-encoding") {
		if (l_value == "chunked") {
			request.chunked_encoding = true;
		} else {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid transfer encoding");
			return 400;
		}
	}

	// Content-Length
	if (l_name == "content-length") {
		std::istringstream iss(value);
		ssize_t parsed_length = -1;
		iss >> parsed_length;
		if (!iss || !iss.eof() || parsed_length < 0) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid Content-Length: " + value);
			return 400;
		}
		request.content_length = parsed_length;
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
			// End of headers
			if (!findHeader(request, "host", logger)) {
				logger.logWithPrefix(Logger::WARNING, "HTTP", "No Host header present");
				return 400;
			}

			if (request.chunked_encoding && findHeader(request, "content-length", logger)) {
				logger.logWithPrefix(Logger::WARNING, "HTTP", "Content-length header present with chunked encoding");
				return 400;
			}

			return 0;
		}

		// Reject line folding (line starts with tab)
		if (!line.empty() && line[0] == '\t') {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Line folding not allowed (tab at line start)");
			return 400;
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

		// Spaces/tabs not allowed in header name
		if (name.find(' ') != std::string::npos || name.find('\t') != std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid header name (contains whitespace)");
			return 400;
		}

		// Valid header name characters
		for (size_t i = 0; i < name.size(); ++i) {
			char c = name[i];
			if (!std::isalnum(c) && c != '-' && c != '_') {
				logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid character in header name: " + name);
				return 400;
			}
		}

		uint16_t header_error = checkHeader(name, value, request, logger);
		if (header_error != 0)
			return header_error;

		request.headers[su::to_lower(name)] = value;
	}

	if (request.content_length == -1 && request.chunked_encoding == false) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "No content length, no chunked encoding");
		return 411;
	}

	logger.logWithPrefix(Logger::WARNING, "HTTP", "Missing final CRLF");
	return 400;
}
