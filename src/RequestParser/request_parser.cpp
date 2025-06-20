/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   request_parser.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/17 15:43:17 by jalombar          #+#    #+#             */
/*   Updated: 2025/06/20 14:59:48 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "request_parser.hpp"
#include "../Logger/Logger.hpp"
#include "../utils/utils.hpp"

bool RequestParsingUtils::assign_method(std::string &method,
                                        ClientRequest &request) {
	if (method == "GET")
		request.method = GET;
	else if (method == "POST")
		request.method = POST;
	else if (method == "DELETE")
		request.method = DELETE_;
	else
		return (false);
	return (true);
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


bool RequestParsingUtils::check_req_line(ClientRequest &request, std::string &method) {
	Logger logger;

	if (method.empty() || request.uri.empty() || request.version.empty()) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Empty component in request line");
		return (false);
	}

	if (method.find(' ') != std::string::npos || request.uri.find(' ') != std::string::npos || request.version.find(' ') != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Extra spaces in request line");
		return (false);
	}
	
	if (!(request.version == "HTTP/1.0" || request.version == "HTTP/1.1")) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid HTTP version");
		return (false);
	}
	
	if (request.uri.length() > MAX_URI_LENGTH) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Uri too big");
		return (false);
	}
	
	if (!RequestParsingUtils::assign_method(method, request)) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid method");
		return (false);
	}
	
	return (true);
}

bool RequestParsingUtils::parse_req_line(std::istringstream &stream,
                                         ClientRequest &request) {
	Logger logger;
	std::string line;
	std::string method;
	logger.logWithPrefix(Logger::INFO, "HTTP", "Parsing request line");

	if (!std::getline(stream, line)) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "No request line present");
		return (false);
	}

	if (!line.empty() && line[line.length() - 1] == '\r')
		line.erase(line.length() - 1);

	std::string trimmed_line = trim_side(line, 3);
	
	// Check for proper format: exactly one space between each component
	size_t first_space = trimmed_line.find(' ');
	if (first_space == std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Request line missing spaces");
		return (false);
	}
	
	size_t second_space = trimmed_line.find(' ', first_space + 1);
	if (second_space == std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Request line missing second space");
		return (false);
	}
	
	// Check for extra spaces between components
	if (first_space == 0 || second_space == first_space + 1) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Extra spaces between request line components");
		return (false);
	}
	
	// Check for trailing spaces or extra spaces after version
	if (trimmed_line.find(' ', second_space + 1) != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Extra spaces after HTTP version");
		return (false);
	}
	
	// Manual parsing to ensure exactly one space between components
	method = trimmed_line.substr(0, first_space);
	request.uri = trimmed_line.substr(first_space + 1, second_space - first_space - 1);
	request.version = trimmed_line.substr(second_space + 1);

	return (RequestParsingUtils::check_req_line(request, method));
}

bool RequestParsingUtils::parse_headers(std::istringstream &stream,
                                        ClientRequest &request) {
	Logger logger;
	std::string line;
	logger.logWithPrefix(Logger::INFO, "HTTP", "Parsing headers");
	int header_count = 0;
	while (std::getline(stream, line)) {
		// Check header count limit
		if (++header_count > 100) {
			logger.logWithPrefix(Logger::WARNING, "HTTP", "Too many headers");
			return (false);
		}

		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);

		if (line.empty())
			break;

		size_t colon = line.find(':');
		if (colon == std::string::npos) {
			logger.logWithPrefix(Logger::WARNING, "HTTP",
			                     "Invalid value in request header");
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

		request.headers[GeneralUtils::to_lower(name)] = value;
	}
	return (true);
}

bool RequestParsingUtils::parse_body(std::istringstream &stream,
                                     ClientRequest &request) {
	Logger logger;
	logger.logWithPrefix(Logger::INFO, "HTTP", "Parsing message body");

	// Check if Content-Lenght Header exists and size is valid
	std::map<std::string, std::string>::iterator it =
	    request.headers.find("content-length");
	if (it == request.headers.end()) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "No Content-Length header present");
		return (false);
	}

	std::istringstream content_stream(it->second);
	int content_length;
	if (!(content_stream >> content_length) || content_length < 0) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Invalid Content-Length value");
		return (false);
	}

	// Read exactly content_length bytes
	std::string body(content_length, '\0');
	stream.read(&body[0], content_length);
	std::streamsize actually_read = stream.gcount();
	if (actually_read != content_length) {
		std::ostringstream msg;
		msg << "Body length mismatch: expected " << content_length
		    << " bytes, but read " << actually_read << " bytes";
		logger.logWithPrefix(Logger::WARNING, "HTTP", msg.str());
		return (false);
	}
	request.body = body;

	return (true);
}

bool RequestParsingUtils::parse_request(const std::string &raw_request,
                                        ClientRequest &request) {
	Logger logger;
	if (raw_request.empty()) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "No request received");
		return (false);
	}

	std::istringstream stream(raw_request);

	// Parse request line
	if (!parse_req_line(stream, request))
		return (false);

	// Parse headers
	if (!parse_headers(stream, request))
		return (false);

	// Read body (rest of the stream)
	if (request.method == POST) {
		if (!parse_body(stream, request))
			return (false);
	}

	// Remove trailing newline if present
	if (!request.body.empty() &&
	    request.body[request.body.length() - 1] == '\n')
		request.body.erase(request.body.length() - 1);

	logger.logWithPrefix(Logger::INFO, "HTTP", "Request parsing completed");
	return (true);
}

/* bool	parse_request(const std::string &raw_request, ClientRequest &request) {
    if (raw_request.empty())
        return (false);

    std::istringstream stream(raw_request);
    std::string line;

    // Parse request line
    if (!std::getline(stream, line))
        return (false);

    // Remove \r if present
    if (!line.empty() && line[line.length() - 1] == '\r')
        line.erase(line.length() - 1);

    std::istringstream request_line(trim(line));
    if (!(request_line >> request.method >> request.uri >> request.version))
        return (false);

    // Parse headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.length() - 1] == '\r')
            line.erase(line.length() - 1);

        if (line.empty()) // Empty line = end of headers
            break;

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            return (false);
                std::string name = trim(line.substr(0, colon));
                std::string value = trim(line.substr(colon + 1));

        // Trim spaces
        if (!value.empty() && value[0] == ' ')
            value.erase(0, 1);

        request.headers[name] = value;
    }

    // Read body (rest of the stream)
    std::ostringstream body_stream;
    while (std::getline(stream, line)) {
        body_stream << line << '\n';
    }
    request.body = body_stream.str();

    // Remove trailing newline if present
    if (!request.body.empty() && request.body[request.body.length() - 1] ==
'\n') request.body.erase(request.body.length() - 1);

    return (true);
} */
