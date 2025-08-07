/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestParser.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/17 15:43:17 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:12:49 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestParser.hpp"

std::string ClientRequest::toString() {
	std::ostringstream oss;

	oss << "ClientRequest {method: " << method << ", ";
	oss << "uri: " << uri << ", ";
	oss << "path: " << path << ", ";
	oss << "query: " << query << ", ";
	oss << "version: " << version << ", ";

	oss << "headers: [";
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
	     it != headers.end(); ++it) {
		oss << it->first << ":" << it->second << ", ";
	}
	oss << "], ";

	oss << "chunked-encoding: " << (chunked_encoding ? "true" : "false") << ", ";

	if (!body.empty()) {
		oss << "body: \"" << body << "\", ";
	} else {
		oss << "body: <empty>, ";
	}

	oss << "clfd: " << clfd << "}";

	return (oss.str());
}

/* Utils */
const char *RequestParsingUtils::findHeader(ClientRequest &request, const std::string &header) {
	std::map<std::string, std::string>::iterator it = request.headers.find(header);
	if (it == request.headers.end())
		return (NULL);
	return (it->second.c_str());
}

// Trim type: 1=left side, 2=right side, 3=both
std::string RequestParsingUtils::trimSide(const std::string &s, int type) {
	std::string result = s;
	if (type == 1 || type == 3) {
		while (!result.empty() && (result[0] == ' ' || result[0] == '\t'))
			result.erase(0, 1);
	}
	if (type == 2 || type == 3) {
		while (!result.empty() &&
		       (result[result.size() - 1] == ' ' || result[result.size() - 1] == '\t'))
			result.erase(result.size() - 1);
	}
	return (result);
}

/* Trailing headers parser */
bool RequestParsingUtils::parseTrailingHeaders(std::istringstream &stream, ClientRequest &request) {
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
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid trailing header");
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
		// Check for valid header to be in trailing
		if (su::to_lower(name) == "te" || su::to_lower(name) == "connection") {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid headers to be in trailing");
			return (false);
		}
		if (!checkHeader(name, value, request))
			return (false);
		request.headers[su::to_lower(name)] = value;
	}
	return (true);
}

bool checkFileUpload(ClientRequest &request) {
	Logger logger;
	bool type = false;
	bool bound = false;

	std::string content_type = request.headers["content-type"];
	if (content_type.find("multipart/form-data") != std::string::npos)
		type = true;
	if (content_type.find("boundary=") != std::string::npos)
		bound = true;
	if (type && bound) {
		request.file_upload = true;
		return (true);
	} else if (type && !bound) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "File upload request missing boundaries");
		return (false);
	} else
		return (true);
}

/* Parser */
bool RequestParsingUtils::parseRequest(const std::string &raw_request, ClientRequest &request) {
	Logger logger;
	if (raw_request.empty()) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "No request received");
		return (false);
	}

	request.chunked_encoding = false;
	request.file_upload = false;
	request.extension = "";
	std::istringstream stream(raw_request);

	// Parse request line
	if (!parseReqLine(stream, request))
		return (false);

	// Parse headers
	if (!parseHeaders(stream, request))
		return (false);

	// Parse body
	if (!parseBody(stream, request))
		return (false);

	// Parse trailing headers (if any)
	if (request.chunked_encoding) {
		if (!parseTrailingHeaders(stream, request))
			return (false);
	}

	// Check if file upload
	if (!checkFileUpload(request))
		return (false);

	logger.logWithPrefix(Logger::INFO, "HTTP", "Request parsing completed");
	return (true);
}
