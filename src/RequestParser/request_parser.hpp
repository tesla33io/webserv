/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   request_parser.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/17 15:32:36 by jalombar          #+#    #+#             */
/*   Updated: 2025/06/19 12:17:56 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef PARSING_HPP
#define PARSING_HPP

# include <iostream>
# include <sstream>
# include <string>
# include <map>
# include <vector>
# include <cstdlib>

enum RequestMethod {
	GET,
	POST,
	DELETE_
};

struct ClientRequest {
	// Request line
    RequestMethod method;
	//std::string method;
    std::string uri;
	std::string version;

	// Headers
	std::map<std::string, std::string> headers;
	
	// Body (optional)
	std::string body;
};

namespace RequestParsingUtils {
	bool assign_method(std::string &method, ClientRequest &request);
	std::string trim(const std::string &s);
	//std::string read_request(int fd);
	bool parse_req_line(std::istringstream &stream, ClientRequest &request);
	bool parse_headers(std::istringstream &stream, ClientRequest &request);
	bool parse_body(std::istringstream &stream, ClientRequest &request);
	bool parse_request(const std::string &raw_request, ClientRequest &request);
}

#endif