/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   valid_directives.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/22 14:19:50 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/25 17:32:55 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config_parser.hpp"


const std::vector<Validity> validDirective = {
	
	{"http",				{"main"},   true, 0, 0},
	{"server",				{"http"}, true, 0, 0},
	{"location",			{"server"}, true, 1, 1},
	
	// subject : Choose the port and host of each ’server’.
	{"listen",				{"server"}, false, 1, 1},
	{"host", 				{"server"}, false, 1, 1}
	// subject : Set up the server_names or not.
	{"server_name",			{"server"}, false, 1, SIZE_MAX}, // one or more names
	// subject : Set up default error pages.
	{"error_page",			{"server", "location"}, false, 2, SIZE_MAX}, // at least 1 code + uri
	// subject : Set the maximum allowed size for client request bodies.
	{"client_max_body_size", {"http", "server", "location"}, false, 1, 1},
	// subject : Define a list of accepted HTTP methods for the route. Make it work with POST and GET methods.
	{"allow_methods",		{"location"}, false, 1, 3}, // GET, POST, DELETE - others are not supported in our webserv
	// subject : Define an HTTP redirect.
	{"return",				{"server", "location"}, false, 1, 1}, // NGINX: code [text|URL] (in NGINX can be more than 1)
	// subject : Define a directory or file where the requested file should be located.
	{"root",				{"http", "server", "location"}, false, 1, 1},
	// subject : Enable or disable directory listing.
	{"autoindex",			{"http", "server", "location"}, false, 1, 1},
	// subject : Execute CGI based on certain file extension
	{"cgi_path",			{"location"}, false, 1, 1}, //path to CGI binary or script interpreter
	{"cgi_ext",				{"location"}, false, 1, SIZE_MAX}, //extensions to match
	// subject : Set a default file to serve when the request is for a directory.
	{"index",				{"http", "server", "location"}, false, 1, SIZE_MAX},
	
};

/********
// OPTIONAL
{"alias", 				{"location"}, false, 1, 1}, // Used to remap /route/foo to some base path wo appending the full URI
{"chunked_transfer_encoding", {"http", "server", "location"}, false, 1, 1}, // if a client is able to receive chunked responses
// OTHER THAT CAN BE INTERESTING
	{"keepalive_timeout",    {"http", "server"}, false, 1, 2}, // timeout [header_timeout]
	{"allow",                {"http", "server", "location"}, false, 1, SIZE_MAX},
	{"deny",                 {"http", "server", "location"}, false, 1, SIZE_MAX},
	{"error_log",            {"main", "http", "server", "location"}, false, 1, 2}, // file [level]
	{"access_log",           {"http", "server", "location"}, false, 1, 2},         // file [format]
	
******/
