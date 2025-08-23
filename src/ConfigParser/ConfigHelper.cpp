/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigHelper.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 13:54:29 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/24 00:23:04 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"
#include "Struct.hpp"


// Remove comments and trim
std::string ConfigParser::preProcess(const std::string &line) const {
	size_t hashpos = line.find('#');
	std::string newline = (hashpos != std::string::npos) ? line.substr(0, hashpos) : line;
	return (su::trim(newline));
}

// isUtil
bool ConfigParser::isBlockStart(const std::string &line) const { return (su::ends_with(line, "{")); }
bool ConfigParser::isBlockEnd(const std::string &line) const { return (line == "}"); }
bool ConfigParser::isDirective(const std::string &line) const { return (su::ends_with(line, ";")); }

// Tokenize
std::vector<std::string> ConfigParser::tokenize(const std::string &line) const {
	std::istringstream iss(line);
	std::vector<std::string> tokens;
	std::string token;
	while (iss >> token)
		tokens.push_back(token);
	return tokens;
}

// Valid IPv4
bool ConfigParser::isValidIPv4(const std::string &ip) {
	std::istringstream iss(ip);
	std::string token;
	int count = 0;
	while (std::getline(iss, token, '.')) {
		if (++count > 4)
			return false;
		if (token.empty())
			return false;
		// Check characters are digits 0-255
		for (size_t i = 0; i < token.length(); ++i) {
			if (!isdigit(token[i]))
				return false;
		}
		int num = atoi(token.c_str());
		if (num < 0 || num > 255)
			return false;
	}
	return (count == 4);
}

// location path, return target uri, upload_path, root
// starts with /, allow common char
bool ConfigParser::isValidUri(const std::string &str) {
	if (str[0] == '/' || (str[0] == '.' && str[1] == '/')) {
		for (size_t i = 1; i < str.length(); ++i) {
			char c = str[i];
			if (!std::isalnum(c) && c != '/' && c != '-' && c != '_' && c != '?' && c != '&' &&
				c != '=' && c != '#' && c != '%' && c != ':' && c != '@' && c != '~') {
				return false;
			}
		}
		return true;
	}
	return false;
}

// index basic validation - no spaces, valid characters
bool ConfigParser::hasOKChar(const std::string &str) {
	for (size_t i = 0; i < str.length(); ++i) {
		char c = str[i];
		if (!std::isalnum(c) && c != '/' && c != '-' && c != '_' && c != '?' && c != '.' &&
			c != '&' && c != '=' && c != '#' && c != '%' && c != ':' && c != '@' && c != '~') {
			return false;
		}
	}
	return true;
}

// check if URL is HTTP/HTTPS
bool ConfigParser::isHttp(const std::string &url) {
	if (url.empty())
		return false;
	if (su::starts_with(url, "http://") && url.size() > 7)
		return true;
	if (su::starts_with(url, "https://") && url.size() > 8)
		return true;
	return false;
}

const int http_status_codes[] = {300, 301, 302, 303, 307, 308, 400, 401, 402, 403, 404, 405,
								 406, 407, 408, 409, 410, 411, 412, 413, 414, 415, 416, 417,
								 418, 421, 422, 423, 424, 425, 426, 428, 429, 431, 451, 500,
								 501, 502, 503, 504, 505, 506, 507, 508, 510, 511};

// Helper function to check if code is in the list
bool ConfigParser::unknownCode(uint16_t code) {
	size_t count = sizeof(http_status_codes) / sizeof(http_status_codes[0]);
	for (size_t i = 0; i < count; ++i) {
		if (http_status_codes[i] == code)
			return false;
	}
	return true;
}

// for multi-context directives (e.g. "server", "location")
std::vector<std::string> ConfigParser::makeVector(const std::string &a, const std::string &b) {
	std::vector<std::string> v;
	v.push_back(a);
	v.push_back(b);
	return v;
}

/////////////
// PRINT TREE
std::string ConfigParser::joinArgs(const std::vector<std::string> &args) {
	if (args.empty())
		return "";

	std::string result = args[0];
	for (size_t i = 1; i < args.size(); ++i) {
		result += " " + args[i];
	}
	return result;
}
void ConfigParser::printTree(const ConfigNode &node, const std::string &prefix, bool isLast,
							 std::ostream &os) const {
	os << prefix;
	os << (isLast ? "└── " : "├── ");
	os << node.name_;

	if (!node.args_.empty()) {
		os << " " << joinArgs(node.args_);
	}

	os << " " << node.line_ << std::endl;

	std::string childPrefix = prefix + (isLast ? "    " : "│   ");

	for (size_t i = 0; i < node.children_.size(); ++i) {
		bool childIsLast = (i == node.children_.size() - 1);
		printTree(node.children_[i], childPrefix, childIsLast, os);
	}
}



////////////////
// PRINT STRUCT
void ConfigParser::printServers(const std::vector<ServerConfig> &servers, std::ostream &os) const {
	os << "=== Configuration Summary ===\n";
	os << "Total servers: " << servers.size() << "\n\n";

	for (size_t i = 0; i < servers.size(); ++i) {
		os << "Server " << (i + 1) << ":\n";
		printServerConfig(servers[i], os);
		os << "\n";
	}
}
void ConfigParser::printLocationConfig(const LocConfig &loc, std::ostream &os) const {
	os << "  Location: " << loc.path;
	os << "\n";

	if (!loc.root.empty())
		os << "    Root: " << loc.root << "\n";

	if (!loc.index.empty()) {
		os << "    Index: " << loc.index << "\n";
	}

	os << "    Max body size: " << su::humanReadableBytes(loc.client_max_body_size) << "\n";

	if (!loc.upload_path.empty()) {
		os << "    Upload path: " << loc.upload_path << "\n";
	}

	os << "    Autoindex: " << (loc.autoindex ? "on" : "off") << "\n";
	os << "    Exact match only: " << (loc.exact_match ? "on" : "off") << "\n";

	if (!loc.allowed_methods.empty()) {
		os << "    Allowed methods: ";
		for (size_t i = 0; i < loc.allowed_methods.size(); ++i)
			os << loc.allowed_methods[i] << (i + 1 < loc.allowed_methods.size() ? ", " : "");
		os << "\n";
	}

	if (loc.hasReturn()) {
		os << "    Return directive: " << loc.return_code;
		if (!loc.return_target.empty())
			os << " " << loc.return_target;
		os << "\n";
	}

	if (!loc.cgi_extensions.empty()) {
		os << "    CGI extensions:\n";
		for (std::map<std::string, std::string>::const_iterator it = loc.cgi_extensions.begin();
			 it != loc.cgi_extensions.end(); ++it) {
			os << "      " << it->first << " -> " << it->second << "\n";
		}
	}
}
void ConfigParser::printServerConfig(const ServerConfig &server, std::ostream &os) const {
	os << "Server on " << server.getHost() << ":" << server.port << "\n";

	if (!server.error_pages.empty()) {
		os << "  Error pages:\n";
		for (std::map<uint16_t, std::string>::const_iterator it = server.error_pages.begin();
			 it != server.error_pages.end(); ++it) {
			os << "    " << it->first << " -> " << it->second << "\n";
		}
	}

	if (!server.locations.empty()) {
		for (size_t i = 0; i < server.locations.size(); ++i) {
			printLocationConfig(server.locations[i], os);
			if (i < server.locations.size() - 1) {
				os << "\n";
			}
		}
	}

	os << "-------------------------------------------\n";
}
