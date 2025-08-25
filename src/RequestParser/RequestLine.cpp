/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestLine.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 10:33:32 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/23 22:44:53 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestParser.hpp"

/* URI checks */
static bool isHexDigit(char ch) {
	return (std::isdigit(ch) || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'));
}

static int hex_value(char ch) {
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return 10 + (ch - 'A');
	if (ch >= 'a' && ch <= 'f')
		return 10 + (ch - 'a');
	return (-1);
}

bool decodeNValidateUri(const std::string &uri, std::string &decoded) {
	decoded.clear();
	decoded.reserve(uri.length());

	for (std::string::size_type i = 0; i < uri.length(); ++i) {
		char ch = uri[i];

		if (ch == '%') {
			// Reject incomplete percent sequence
			if (i + 2 >= uri.length())
				return (false);

			char hi = uri[i + 1];
			char lo = uri[i + 2];

			// Reject invalid hex digits
			if (!isHexDigit(hi) || !isHexDigit(lo))
				return (false);

			int decoded_char = (hex_value(hi) << 4) | hex_value(lo);

			// Reject control chars and DEL
			if (decoded_char <= 0x1F || decoded_char == 0x7F)
				return (false);

			decoded += static_cast<char>(decoded_char);
			i += 2;
		} else {
			unsigned char uch = static_cast<unsigned char>(ch);

			// Disallow unescaped control characters, DEL, space, quotes, backslash
			if (uch <= 0x1F || uch == 0x7F || uch == ' ' || uch == '"' || uch == '\'' ||
			    uch == '\\')
				return (false);

			// Disallow non-ASCII
			if (uch > 0x7E)
				return (false);

			// CRLF injection prevention
			if (ch == '\r' || ch == '\n')
				return (false);

			decoded += ch;
		}
	}

	return (true);
}

// Additional function to validate query parameters specifically
bool validateQueryString(const std::string &query) {
	for (size_t i = 0; i < query.length(); ++i) {
		unsigned char ch = static_cast<unsigned char>(query[i]);
		
		// Check for non-ASCII characters
		if (ch > 0x7E)
			return false;
			
		// Check for unescaped control characters
		if (ch <= 0x1F || ch == 0x7F)
			return false;
			
		// CRLF injection prevention
		if (ch == '\r' || ch == '\n')
			return false;
	}
	return true;
}

/* Checks */
uint16_t RequestParsingUtils::checkReqLine(ClientRequest &request, Logger &logger) {

	if (request.method.empty() || request.uri.empty() || request.version.empty()) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Empty component in request line");
		return 400;
	}

	// CRLF injection check in method, uri, version
	if (request.method.find('\r') != std::string::npos || 
	    request.method.find('\n') != std::string::npos ||
	    request.uri.find('\r') != std::string::npos ||
	    request.uri.find('\n') != std::string::npos ||
	    request.version.find('\r') != std::string::npos ||
	    request.version.find('\n') != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "CRLF injection attempt detected");
		return 400;
	}

	if (request.method.find(' ') != std::string::npos ||
	    request.uri.find(' ') != std::string::npos ||
	    request.version.find(' ') != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Extra spaces in request line");
		return 400;
	}

	if (request.method != "POST" && request.method != "GET" && request.method != "DELETE") {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Unsupported HTTP method: " + request.method);
		return 501;
	}

	if (request.uri.length() > MAX_URI_LENGTH) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Uri too big");
		return 414;
	}
	
	std::string decoded_uri;
	if (!decodeNValidateUri(request.uri, decoded_uri)) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid uri");
		return 400;
	}
	request.uri = decoded_uri;
	
	size_t qm = request.uri.find_first_of('?');
	if (qm != std::string::npos) {
		request.path = request.uri.substr(0, qm);
		request.query = request.uri.substr(qm + 1);
		
		// Validate query string for non-ASCII characters
		if (!validateQueryString(request.query)) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid characters in query string");
			return 400;
		}
	} else {
		request.path = request.uri;
		request.query = "";
	}
	
	// case 505: "HTTP Version Not Supported"
	if (request.version != "HTTP/1.1") {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid HTTP version");
		return 505;
	}

	return 0;
}

/* Parsing */
uint16_t RequestParsingUtils::parseReqLine(std::istringstream &stream, ClientRequest &request,
                                           Logger &logger) {
	std::string line;
	logger.logWithPrefix(Logger::DEBUG, "HTTP", "Parsing request line");

	if (!std::getline(stream, line)) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "No request line present");
		return 400;
	}

	if (!line.empty() && line[line.length() - 1] == '\r')
		line.erase(line.length() - 1);

	std::string trimmed_line = trimSide(line, 3);

	// Check for proper format: exactly one space between each component
	size_t first_space = trimmed_line.find(' ');
	if (first_space == std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Request line missing spaces");
		return 400;
	}

	size_t second_space = trimmed_line.find(' ', first_space + 1);
	if (second_space == std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid request line format");
		return 400;
	}

	// Check for extra spaces between components
	if (first_space == 0 || second_space == first_space + 1) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Extra spaces between request line components");
		return 400;
	}

	// Check for trailing spaces or extra spaces after version
	if (trimmed_line.find(' ', second_space + 1) != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Extra spaces after HTTP version");
		return 400;
	}

	// Manual parsing to ensure exactly one space between components
	request.method = trimmed_line.substr(0, first_space);
	request.uri = trimmed_line.substr(first_space + 1, second_space - first_space - 1);
	request.version = trimmed_line.substr(second_space + 1);

	return (RequestParsingUtils::checkReqLine(request, logger));
}
