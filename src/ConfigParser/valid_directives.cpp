/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   valid_directives.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/22 14:19:50 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/23 19:36:04 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config_parser.hpp"


const std::vector<Validity> validDirective = {
	
	// Server-level directives
	{"listen",      {"server"}, false, 1, 1},
	{"server_name", {"server"}, false, 1, SIZE_MAX}, // one or more names
	{"location", {"server"}, true, 1, 1},

	// Directives valid in http/server/location (inheritance possible)
	{"root",                 {"http", "server", "location"}, false, 1, 1},
	{"index",                {"http", "server", "location"}, false, 1, SIZE_MAX},
	{"autoindex",            {"http", "server", "location"}, false, 1, 1},
	{"error_page",           {"http", "server", "location"}, false, 2, SIZE_MAX}, // at least 1 code + uri
	{"client_max_body_size", {"http", "server", "location"}, false, 1, 1},

	// Location-only directives
	{"allow_methods",{"location"}, false, 1, SIZE_MAX}, // GET, POST, DELETE, etc.
	{"return",       {"server", "location"}, false, 1, SIZE_MAX}, // NGINX: code [text|URL]
	{"cgi_path",     {"location"}, false, 1, 1},        // custom, not standard NGINX
	{"cgi_ext",      {"location"}, false, 1, SIZE_MAX}, // custom, not standard NGINX

};

/********
 
	{"http",   {"main"},   true, 0, 0},
	{"server", {"http"},   true, 0, 0},
	{"chunked_transfer_encoding", {"http", "server", "location"}, false, 1, 1}, // not standard NGINX
	{"host",        {"server"}, false, 1, 1},        // custom, not standard NGINX
	{"keepalive_timeout",    {"http", "server"}, false, 1, 2}, // timeout [header_timeout]
	{"allow",                {"http", "server", "location"}, false, 1, SIZE_MAX},
	{"deny",                 {"http", "server", "location"}, false, 1, SIZE_MAX},
	{"alias",        {"location"}, false, 1, 1},
	{"error_log",            {"main", "http", "server", "location"}, false, 1, 2}, // file [level]
	{"access_log",           {"http", "server", "location"}, false, 1, 2},         // file [format]
	
******/
