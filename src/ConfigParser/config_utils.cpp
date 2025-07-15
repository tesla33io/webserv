/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_utils.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/22 15:53:58 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/24 15:18:14 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config_parser.hpp"
#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

namespace ConfigParsing {

	// Remove comments and trim
	std::string preProcess(const std::string& line) {
		size_t hashpos = line.find('#');
		std::string newline = (hashpos != std::string::npos) ? line.substr(0, hashpos) : line;
		return (su::trim(newline));
	}

	// isUtil
	bool isConfigNodeStart(const std::string& line) {
		return (su::ends_with(line, "{"));
	}
	bool isConfigNodeEnd(const std::string& line) {
		return (line == "}") ;
	}
	bool isDirective(const std::string& line) {
		return (su::ends_with(line, ";"));
	}

	// Tokenize
	std::vector<std::string> tokenize(const std::string& line) {
		std::istringstream iss(line);
		std::vector<std::string> tokens;
		std::string token;
		while (iss >> token) {
			tokens.push_back(token);
		}
		return tokens;
	}

	// Valid IPv4
	bool isValidIPv4(const std::string& ip) {
		std::istringstream iss(ip);
		std::string token;
		int count = 0;
		while (std::getline(iss, token, '.')) { // reads from the stringstream until it hits a '.' character
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


	// Print function for the tree
	std::string join_args(const std::vector<std::string>& args) {
		
		std::string out = "[";
		for (size_t i = 0; i < args.size(); ++i) {
			out += "\"" + args[i] + "\"";
			if (i + 1 < args.size()) out += ", ";
		}
		out += "]";
		
		return out;
	}
	void print_tree_config(const ConfigNode& node, const std::string& prefix, bool isLast, std::ostream &os) {
	
		os << prefix;
		os << (isLast ? "└── " : "├── ");
		os << node.name;
		if (!node.args.empty()) {
			os << " " << join_args(node.args) ;
		}
		os << "  " << node.lineNumber << std::endl;

		std::string childPrefix = prefix + (isLast ? "    " : "│   ");

		for (size_t i = 0; i < node.children.size(); ++i) {
			bool childIsLast = (i == node.children.size() - 1);
			print_tree_config(node.children[i], childPrefix, childIsLast, os);
		}
	}

	// Print function for the struct
	void print_location_config(const LocConfig &loc, std::ostream &os) {
		os << "  Location: " << loc.path << "\n";
		if (!loc.root.empty())
			os << "    Root: " << loc.root << "\n";
		if (!loc.index.empty()) {
			os << "    Index: ";
			for (size_t i = 0; i < loc.index.size(); ++i)
				os << loc.index[i] << (i + 1 < loc.index.size() ? ", " : "");
			os << "\n";
		}
		os << "    Autoindex: " << (loc.autoindex ? "on" : "off") << "\n";
		if (!loc.limit_except.empty()) {
			os << "    Allowed methods: ";
			for (size_t i = 0; i < loc.limit_except.size(); ++i)
				os << loc.limit_except[i] << (i + 1 < loc.limit_except.size() ? ", " : "");
			os << "\n";
		}
		if (loc.ret_dir.is_set) {
			os << "    Return directive: ";
			os << loc.ret_dir.status_code;
			if (!loc.ret_dir.uri.empty())
				os << " " << loc.ret_dir.uri;
			os << "\n";
		}
		if (!loc.cgi_ext.empty()) {
			os << "    CGI extensions: ";
			for (size_t i = 0; i < loc.cgi_ext.size(); ++i)
				os << loc.cgi_ext[i] << (i + 1 < loc.cgi_ext.size() ? ", " : "");
			os << "\n";
		}
		if (!loc.error_pages.empty()) {
			os << "    Error pages:\n";
			for (std::map<int, std::string>::const_iterator it = loc.error_pages.begin(); it != loc.error_pages.end(); ++it)
				os << "      " << it->first << " -> " << it->second << "\n";
		}
		os << "    Client max body size: " << loc.client_max_body_size << "\n";
	}
	void print_server_config(const ServerConfig &server, std::ostream &os) {
		os << "Server on " << server.host << ":" << server.port << "\n";
		os << "  Server names: ";
		for (size_t i = 0; i < server.server_names.size(); ++i)
			os << server.server_names[i] << (i + 1 < server.server_names.size() ? ", " : "");
		os << "\n";
		for (size_t i = 0; i < server.locations.size(); ++i)
			print_location_config(server.locations[i], os);
		os << "-------------------------------------------\n";
	}

	
}


