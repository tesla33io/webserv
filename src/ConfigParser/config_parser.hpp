/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_parser.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/21 14:53:33 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/25 18:41:56 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <vector>
#include <map>
#include <string>
#include <limits>
#include <iostream>
#include <exception>
#include <fstream>
#include <sstream>

#include "../Logger/Logger.hpp"

struct Validity {
	std::string name;
	std::vector<std::string> contexts;
	bool isConfigNode;
	size_t minArgs; 
	size_t maxArgs; 
};

struct ConfigNode {
	std::string name;
	std::vector<std::string> args;
	std::vector<ConfigNode> children;
	int lineNumber;
};

struct LocConfig {
	std::string 				path;
	std::vector<std::string>	allow_methods;
	std::vector<std::string>	cgi_ext;
	std::string 				cgi_path;
	std::map<int, std::string>	error_pages;
	size_t 						client_max_body_size ;
	std::string					return_url;
	std::string 				root;
	bool 						autoindex;
	std::vector<std::string>  	index;
	// std::string 				alias;
	// bool 					chunked_encoding; // = false  // only needed for streaming

	LocConfig() :	path(""),
		 			allow_methods(),
		 			cgi_ext(),
		 			cgi_path(""),
		 			error_pages(),
		 			client_max_body_size(0), // DEFAULT = 0 meaning unlimited
		 			return_url(""),
		 			root(""),
		 			autoindex(false),  // DEFAULT = false
		 			index()
					{}
} ; 
   
struct ServerConfig {
	std::string 				host;
	int 						port; 
	std::vector<std::string> 	server_names;
	std::vector<LocConfig> 		locations;

	ServerConfig() :	host("0.0.0.0"),// additional checks/diff default if duplicate? no implemented
						port(8080), // = 8080;
						server_names() 
						{}
} ;


namespace ConfigParsing {

	bool tree_parser(const std::string &filePath, ConfigNode &childNode, Logger &logger);
	bool parse_config(std::ifstream &file, int &line_nb, ConfigNode &parent, Logger& logger);
	void struct_parser(const ConfigNode &tree, std::vector<ServerConfig> &servers, Logger &logger);
	void handle_location(LocConfig &location, const std::vector<ConfigNode> &children);
	void handle_loc_details(LocConfig &location, const ConfigNode &child);
	void handle_error_page(std::map<int, std::string>& error_pages, const std::vector<std::string>& args);
	void inherit_gen_dir(ServerConfig &server, const LocConfig &general_dir);
	void inherit_error_pages(std::map<int, std::string>& loc_map, const std::map<int, std::string>& general_map);

	std::string preProcess(const std::string& line);
	std::vector<std::string> tokenize(const std::string& line);
	bool isConfigNodeStart(const std::string& line);
	bool isConfigNodeEnd(const std::string& line);
	bool isDirective(const std::string& line);
	std::string join_args(const std::vector<std::string>& args);
	void print_tree_config(const ConfigNode& node, const std::string& prefix, bool isLast, std::ostream &os);
	void print_location_config(const LocConfig &loc, std::ostream &os);
	void print_server_config(const ServerConfig &server, std::ostream &os);

}

#endif
