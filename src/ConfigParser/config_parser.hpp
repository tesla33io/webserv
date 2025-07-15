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
#include <iostream>
#include <exception>
#include <fstream>
#include <sstream>

#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

struct ConfigNode {
	std::string name;
	std::vector<std::string> args;
	std::vector<ConfigNode> children;
	int lineNumber;
	void *f;
};

typedef bool (*ValidFtnPnter)(const ConfigNode&, Logger&);

struct Validity {
	std::string name;
	std::vector<std::string> contexts; 
	bool repeatOK;
	size_t minArgs;
	size_t maxArgs;
	ValidFtnPnter validF;

	Validity(std::string n, std::vector<std::string> c, bool rep, size_t min, size_t max, bool (*f)(const ConfigNode&, Logger&))
							: 	name(n), 
								contexts(c), 
								repeatOK(rep),  
								minArgs(min), 
								maxArgs(max), 
								validF(f) 
								{}
};

struct ReturnDir {
	unsigned int status_code;
	std::string uri;
	bool is_set;

	ReturnDir() : status_code(0), 
						uri(""), 
						is_set(false) 
						{}
};

struct LocConfig {
	std::string 				path;
	std::string 				root;
	ReturnDir		 			ret_dir;
	std::string 				alias; 
	std::vector<std::string>	limit_except;
	std::vector<std::string>	cgi_ext;
	std::map<int, std::string>	error_pages;
	size_t 						client_max_body_size ;
	bool						autoindex;
	std::vector<std::string>  	index;
	bool 						chunked_encoding;

	LocConfig() :	path(""),
					root(""),
					ret_dir(),
					alias(""),
		 			limit_except(),
		 			cgi_ext(),
		 			error_pages(),
		 			client_max_body_size(0), // DEFAULT = 0 meaning unlimited
		 			autoindex(false),  // DEFAULT = false
		 			index(),
					chunked_encoding(false) // DEFAULT = false
					{}
} ; 
   
struct ServerConfig {
	std::string 				host;
	int 						port; 
	std::vector<std::string> 	server_names;
	std::vector<LocConfig> 		locations;

	ServerConfig() :	host("0.0.0.0"),// additional checks/diff default if duplicate? no implemented
						port(8080), // DEF 8080;
						server_names() 
						{}
} ;


namespace ConfigParsing {

	bool tree_parser(const std::string &filePath, ConfigNode &childNode, Logger &logger);
	bool tree_parser_blocks(std::ifstream &file, int &line_nb, ConfigNode &parent, Logger& logger);
	void struct_parser(const ConfigNode &tree, std::vector<ServerConfig> &servers, Logger &logger);
	void handle_location(LocConfig &location, const std::vector<ConfigNode> &children);
	void handle_loc_details(LocConfig &location, const ConfigNode &child);
	void handle_listen(const ConfigNode& node, std::string& host, int& port);
	void handle_error_page(std::map<int, std::string>& error_pages, const std::vector<std::string>& args);
	void handle_return(LocConfig &location, const ConfigNode &node);
	void inherit_gen_dir(ServerConfig &server, const LocConfig &general_dir);
	void inherit_error_pages(std::map<int, std::string>& loc_map, const std::map<int, std::string>& general_map);
	std::string preProcess(const std::string& line);
	std::vector<std::string> tokenize(const std::string& line);
	bool isConfigNodeStart(const std::string& line);
	bool isConfigNodeEnd(const std::string& line);
	bool isDirective(const std::string& line);
	bool isValidIPv4(const std::string& ip);
	std::string join_args(const std::vector<std::string>& args);
	void print_tree_config(const ConfigNode& node, const std::string& prefix, bool isLast, std::ostream &os);
	void print_location_config(const LocConfig &loc, std::ostream &os);
	void print_server_config(const ServerConfig &server, std::ostream &os);
	bool validateListen(const ConfigNode& node, Logger& logger);
	bool validateError(const ConfigNode& node, Logger& logger);
	bool validateReturn(const ConfigNode& node, Logger& logger);
	bool validateMaxBody(const ConfigNode& node, Logger& logger);
	bool validateMethod(const ConfigNode& node, Logger& logger);
	bool validateAutoIndex(const ConfigNode& node, Logger& logger);
	bool validateExt(const ConfigNode& node, Logger& logger);
	bool validateChunk(const ConfigNode& node, Logger& logger);
	bool validateDirective(ConfigNode& childNode, ConfigNode& parent, Logger& logger);

}

#endif
