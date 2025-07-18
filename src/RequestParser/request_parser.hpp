/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   request_parser.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/17 15:32:36 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/18 11:41:38 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

# include "../../includes/webserv.hpp"
# include "../../includes/types.hpp"
#include "../Logger/Logger.hpp"
#include "../Utils/GeneralUtils.hpp"
# include <iostream>
# include <sstream>
# include <string>
# include <map>
# include <vector>
# include <cstdlib>

const size_t MAX_URI_LENGTH = 2048;
const size_t MAX_HEADER_NAME_LENGTH = 1024;
const size_t MAX_HEADER_VALUE_LENGTH = 8000;

/* enum RequestMethod { GET, POST, DELETE_ };

struct ClientRequest {
	// Request line
	RequestMethod method;
	std::string uri;
	std::string version;

	// Headers
	std::map<std::string, std::string> headers;
	bool chunked_encoding;

	// Body (optional)
	std::string body;

	// Client FD
	int clfd;
}; */

namespace RequestParsingUtils {
	bool check_and_trim_line(std::string &line);
	const char *find_header(ClientRequest &request, const std::string &header);
	bool assign_method(std::string &method, ClientRequest &request);
	std::string trim_side(const std::string &s, int type);
	bool check_req_line(ClientRequest &request, std::string &method);
	bool parse_req_line(std::istringstream &stream, ClientRequest &request);
	bool check_header(std::string &name, std::string &value, ClientRequest &request);
	bool parse_headers(std::istringstream &stream, ClientRequest &request);
	bool chunked_encoding(std::istringstream &stream, ClientRequest &request);
	bool parse_body(std::istringstream &stream, ClientRequest &request);
	bool parse_trailing_headers(std::istringstream &stream, ClientRequest &request);
	bool parse_request(const std::string &raw_request, ClientRequest &request);
} // namespace RequestParsingUtils

#endif
