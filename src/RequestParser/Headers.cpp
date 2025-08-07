/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Headers.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 10:46:05 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:18:06 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestParser.hpp"

/* Checks */
bool RequestParsingUtils::checkHeader(std::string &name, std::string &value,
                                      ClientRequest &request) {
	Logger logger;

	// Check header size
	if (name.size() > MAX_HEADER_NAME_LENGTH) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Header name too big");
		return (false);
	} else if (value.size() > MAX_HEADER_VALUE_LENGTH) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Header value too big");
		return (false);
	}
	// Check for duplicate header
	if (findHeader(request, su::to_lower(name))) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Duplicate header present");
		return (false);
	}
	if (su::to_lower(name) == "transfer-encoding" && su::to_lower(value) == "chunked")
		request.chunked_encoding = true;
	return (true);
}

/* Parser */
bool RequestParsingUtils::parseHeaders(std::istringstream &stream, ClientRequest &request) {
	Logger logger;
	std::string line;
	logger.logWithPrefix(Logger::DEBUG, "HTTP", "Parsing headers");
	int header_count = 0;
	while (std::getline(stream, line)) {
		// Check header count limit
		if (++header_count > 100) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Too many headers");
			return (false);
		}

		if (!checkNTrimLine(line))
			return (false);

		if (line.empty()) {
			// Check for host header
			if (!findHeader(request, "host")) {
				logger.logWithPrefix(Logger::WARNING, "HTTP", "No Host header present");
				return (false);
			}
			// Check for Transfer-encoding=chunked and Content-length headers
			if (request.chunked_encoding && findHeader(request, "content-length")) {
				logger.logWithPrefix(Logger::WARNING, "HTTP",
				                     "Content-length header present with chunked encoding");
				return (false);
			}
			return (true);
		}

		size_t colon = line.find(':');
		if (colon == std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid header format");
			return (false);
		}
		std::string name = trimSide(line.substr(0, colon), 1);
		std::string value = trimSide(line.substr(colon + 1), 3);

		if (name.empty()) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Empty header name");
			return (false);
		}
		// Check for spaces in header name (invalid according to HTTP spec)
		if (name.find(' ') != std::string::npos || name.find('\t') != std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid header name (contains spaces)");
			return (false);
		}
		// Check for valid header name characters
		for (size_t i = 0; i < name.length(); ++i) {
			char c = name[i];
			if (!std::isalnum(c) && c != '-' && c != '_') {
				logger.logWithPrefix(Logger::WARNING, "HTTP",
				                     "Invalid character in header name: " + name);
				return (false);
			}
		}
		if (!checkHeader(name, value, request))
			return (false);
		request.headers[su::to_lower(name)] = value;
	}
	logger.logWithPrefix(Logger::WARNING, "HTTP", "Missing final CRLF");
	return (false);
}
