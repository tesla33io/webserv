/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   request_line.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 10:33:32 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/18 17:30:23 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../Logger/Logger.hpp"
#include "request_parser.hpp"

/* Utils */
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

/* URI checks */
static bool is_hex_digit(char ch) {
	return (std::isdigit(ch) || (ch >= 'A' && ch <= 'F') ||
	        (ch >= 'a' && ch <= 'f'));
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

bool decode_and_validate_uri(const std::string &uri, std::string &decoded) {
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
			if (!is_hex_digit(hi) || !is_hex_digit(lo))
				return (false);

			int decoded_char = (hex_value(hi) << 4) | hex_value(lo);

			// Reject control chars and DEL
			if (decoded_char <= 0x1F || decoded_char == 0x7F)
				return (false);

			decoded += static_cast<char>(decoded_char);
			i += 2;
		} else {
			unsigned char uch = static_cast<unsigned char>(ch);

			// Disallow unescaped control characters, DEL, space, quotes,
			// backslash
			if (uch <= 0x1F || uch == 0x7F || uch == ' ' || uch == '"' ||
			    uch == '\'' || uch == '\\')
				return (false);

			// Disallow non-ASCII
			if (uch > 0x7E)
				return (false);

			decoded += ch;
		}
	}

	return (true);
}

/* Checks */
bool RequestParsingUtils::check_req_line(ClientRequest &request,
                                         std::string &method) {
	Logger logger;

	if (method.empty() || request.uri.empty() || request.version.empty()) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Empty component in request line");
		return (false);
	}

	if (method.find(' ') != std::string::npos ||
	    request.uri.find(' ') != std::string::npos ||
	    request.version.find(' ') != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Extra spaces in request line");
		return (false);
	}

	if (!RequestParsingUtils::assign_method(method, request)) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid method");
		return (false);
	}

	if (request.uri.length() > MAX_URI_LENGTH) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Uri too big");
		return (false);
	}
	std::string decoded_uri;
	if (!decode_and_validate_uri(request.uri, decoded_uri)) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid uri");
		return (false);
	}
	request.uri = decoded_uri;
	size_t qm = request.uri.find('?');
	if (qm != std::string::npos) {
		request.path = request.uri.substr(0, qm);
		request.query = request.uri.substr(qm + 1);
	} else {
		request.path = request.uri;
		request.query = "";
	}

	if (!strncmp(request.path.c_str(), "cgi-bin", 7))
		request.CGI = true;

	if (!(request.version == "HTTP/1.0" || request.version == "HTTP/1.1")) {
		logger.logWithPrefix(Logger::WARNING, "HTTP", "Invalid HTTP version");
		return (false);
	}

	return (true);
}

/* Parsing */
bool RequestParsingUtils::parse_req_line(std::istringstream &stream,
                                         ClientRequest &request) {
	Logger logger;
	std::string line;
	std::string method;
	logger.logWithPrefix(Logger::INFO, "HTTP", "Parsing request line");

	if (!std::getline(stream, line)) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "No request line present");
		return (false);
	}

	if (!line.empty() && line[line.length() - 1] == '\r')
		line.erase(line.length() - 1);

	std::string trimmed_line = trim_side(line, 3);

	// Check for proper format: exactly one space between each component
	size_t first_space = trimmed_line.find(' ');
	if (first_space == std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Request line missing spaces");
		return (false);
	}

	size_t second_space = trimmed_line.find(' ', first_space + 1);
	if (second_space == std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Invalid request line format");
		return (false);
	}

	// Check for extra spaces between components
	if (first_space == 0 || second_space == first_space + 1) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Extra spaces between request line components");
		return (false);
	}

	// Check for trailing spaces or extra spaces after version
	if (trimmed_line.find(' ', second_space + 1) != std::string::npos) {
		logger.logWithPrefix(Logger::WARNING, "HTTP",
		                     "Extra spaces after HTTP version");
		return (false);
	}

	// Manual parsing to ensure exactly one space between components
	method = trimmed_line.substr(0, first_space);
	request.uri =
	    trimmed_line.substr(first_space + 1, second_space - first_space - 1);
	request.version = trimmed_line.substr(second_space + 1);

	return (RequestParsingUtils::check_req_line(request, method));
}
