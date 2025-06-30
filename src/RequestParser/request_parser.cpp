/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   request_parser.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/17 15:43:17 by jalombar          #+#    #+#             */
/*   Updated: 2025/06/26 10:10:34 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "request_parser.hpp"
#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

/* Utils */
const char *RequestParsingUtils::find_header(ClientRequest &request,
                                             const std::string &header) {
	std::map<std::string, std::string>::iterator it =
	    request.headers.find(header);
	if (it == request.headers.end())
		return (NULL);
	return (it->second.c_str());
}

// Trim type: 1=left side, 2=right side, 3=both
std::string RequestParsingUtils::trim_side(const std::string &s, int type) {
	std::string result = s;
	if (type == 1 || type == 3) {
		while (!result.empty() && (result[0] == ' ' || result[0] == '\t'))
			result.erase(0, 1);
	}
	if (type == 2 || type == 3) {
		while (!result.empty() && (result[result.size() - 1] == ' ' ||
		                           result[result.size() - 1] == '\t'))
			result.erase(result.size() - 1);
	}
	return (result);
}

/* Trailing headers parser */
bool RequestParsingUtils::parse_trailing_headers(std::istringstream &stream,
                                                 ClientRequest &request) {
	Logger logger;
	std::string line;
	logger.logWithPrefix(Logger::INFO, "HTTP", "Parsing trailing headers");

	while (std::getline(stream, line)) {
		// Check if end of request
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);

		if (line.empty())
			return (true);

		size_t colon = line.find(':');
		if (colon == std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "HTTP",
			                     "Invalid trailing header");
			return (false);
		}
		std::string name = trim_side(line.substr(0, colon), 1);
		std::string value = trim_side(line.substr(colon + 1), 3);

		if (name.empty()) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Empty header name");
			return (false);
		}
		// Check for spaces in header name (invalid according to HTTP spec)
		if (name.find(' ') != std::string::npos ||
		    name.find('\t') != std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "HTTP",
			                     "Invalid header name (contains spaces)");
			return (false);
		}
		// Check for valid header name characters
		for (size_t i = 0; i < name.length(); ++i) {
			char c = name[i];
			if (!std::isalnum(c) && c != '-' && c != '_') {
				logger.logWithPrefix(Logger::WARNING, "HTTP",
				                     "Invalid character in header name: " +
				                         name);
				return (false);
			}
		}
		// Check for valid header to be in trailing
		if (string_utils::to_lower(name) == "te" ||
		    string_utils::to_lower(name) == "connection") {
			logger.logWithPrefix(Logger::WARNING, "HTTP",
			                     "Invalid headers to be in trailing");
			return (false);
		}
		if (!check_header(name, value, request))
			return (false);
		request.headers[string_utils::to_lower(name)] = value;
	}
	return (true);
}

/* Parser */
bool RequestParsingUtils::parse_request(const std::string &raw_request,
                                        ClientRequest &request) {
	Logger logger;
	if (raw_request.empty()) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "No request received");
		return (false);
	}

	request.chunked_encoding = false;
	std::istringstream stream(raw_request);

	// Parse request line
	if (!parse_req_line(stream, request))
		return (false);

	// Parse headers
	if (!parse_headers(stream, request))
		return (false);

	// Parse body
	if (!parse_body(stream, request))
		return (false);

	// Parse trailing headers (if any)
	if (request.chunked_encoding) {
		if (!parse_trailing_headers(stream, request))
			return (false);
	}

	logger.logWithPrefix(Logger::INFO, "HTTP", "Request parsing completed");
	return (true);
}
