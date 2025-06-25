/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_parser.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/21 14:53:33 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/25 17:47:02 by htharrau         ###   ########.fr       */
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

#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

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
	std::vector<std::string>  	allow_methods;
	std::vector<std::string>  	cgi_ext;
	std::string 				cgi_path;
	std::map<int, std::string>  error_pages;
	std::string 				alias;
	bool 						chunked_encoding = false; // only needed for streaming
	size_t 						client_max_body_size = 0; // meaning unlimited
	std::string					return_url;
	std::string 				root;
	bool 						autoindex = false;
	std::vector<std::string>  	index;

} ;

struct ServerConfig {
	std::string 				host = "0.0.0.0";
	int 						port = 8080;
	std::vector<std::string> 	server_names;
	std::vector<LocConfig> 		locations;
} ;


namespace ConfigParsing {

bool parser(const std::string &filePath, ConfigNode &ConfigNode);
bool parse_config(std::ifstream &file, ConfigNode &parent, Logger& logger);
std::string preProcess(const std::string& line);
std::vector<std::string> tokenize(const std::string& line);
bool isConfigNodeStart(const std::string& line);
bool isConfigNodeEnd(const std::string& line);
bool isDirective(const std::string& line);
std::string join_args(const std::vector<std::string>& args);
void pretty_print(const ConfigNode& node, const std::string& prefix = "", bool isLast = true);


}

#endif
