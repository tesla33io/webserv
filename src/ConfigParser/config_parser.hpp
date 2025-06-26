/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_parser.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/21 14:53:33 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/23 19:42:14 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <vector>
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
	bool isBlock;
	size_t minArgs; 
	size_t maxArgs; 
};

struct ConfigNode {
	std::string name;
	std::vector<std::string> args;
	std::vector<ConfigNode> children;
	int lineNumber;
};

namespace ConfigParsing {


bool parser(const std::string &filePath, ConfigNode &ConfigNode);
bool parse_config(std::ifstream &file, ConfigNode &parent, Logger& logger);
std::string preProcess(const std::string& line);
std::vector<std::string> tokenize(const std::string& line);
bool isBlockStart(const std::string& line);
bool isBlockEnd(const std::string& line);
bool isDirective(const std::string& line);
std::string join_args(const std::vector<std::string>& args);
void pretty_print(const ConfigNode& node, const std::string& prefix = "", bool isLast = true);


}

#endif
