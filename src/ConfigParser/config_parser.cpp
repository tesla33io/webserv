/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_parser.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/21 14:53:37 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/23 19:52:04 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/*//////////////////////////////////////////////////////////////////////////////
// NGINX ERROR MESSAGES

[emerg] unexpected "}" in /etc/nginx/nginx.conf:45
→ You closed a block that wasn’t open.

[emerg] unexpected end of file, expecting "}" in /etc/nginx/nginx.conf:67
→ You forgot to close a block (e.g., server { with no matching }).

[emerg] invalid number of arguments in "listen" directive in /etc/nginx/nginx.conf:20
→ The directive was given too many or too few arguments.

[emerg] directive "server" is not terminated by ";" in /etc/nginx/nginx.conf:21
→ You forgot the semicolon at the end of a directive.

[emerg] "listen" directive is not allowed here in /etc/nginx/nginx.conf:13
→ You probably put listen outside of a server block.

[emerg] duplicate "listen" directive in /etc/nginx/nginx.conf:25
→ You defined listen more than once in a server block without using default_server, etc.

[emerg] a duplicate default server for 0.0.0.0:80 in /etc/nginx/nginx.conf:35
→ You have more than one default_server for the same address/port.

[emerg] open() "/etc/nginx/mime.types" failed (2: No such file or directory)
→ You referenced a file that doesn’t exist (e.g., with include).

[emerg] unknown directive "slurp" in /etc/nginx/nginx.conf:12
→ You used a directive that NGINX doesn’t recognize (typo or unsupported module).

[emerg] invalid port in "listen" directive in /etc/nginx/nginx.conf:16
→ You gave a non-numeric or invalid port.

/*/////////////////////////////////////////////////////////////////////////////*

#include "config_parser.hpp"
#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

// struct ConfigNode {
// 	std::string name;
// 	std::vector<std::string> args;
// 	std::vector<ConfigNode> children;
// 	int lineNumber;
// };

// struct Validity {
// 	std::string name;
// 	std::vector<std::string> contexts;
// 	bool nested;
// 	size_t maxArgs; 
// };


// bool parser(const std::string &filePath, ConfigNode &ConfigNode);
// bool parse_config(std::ifstream &file, ConfigNode &parent, Logger& logger);
// std::string preProcess(const std::string& line);
// std::vector<std::string> tokenize(const std::string& line);
// bool isBlockStart(const std::string& line);
// bool isBlockEnd(const std::string& line);
// bool isDirective(const std::string& line);
// std::string join_args(const std::vector<std::string>& args);
// void pretty_print(const ConfigNode& node, const std::string& prefix = "", bool isLast = true);



namespace ConfigParsing {

	bool parser(const std::string &filePath, ConfigNode &ConfigNode) {
		Logger logger;
		std::ifstream confFile(filePath.c_str());
		if (!confFile.is_open()) {
			logger.logWithPrefix(Logger::WARNING, "Configuration file", "Could not open file: " + filePath);
			return false;
		}
		ConfigNode.lineNumber = 0;
		ConfigNode.name = "main";
		return (ConfigParsing::parse_config(confFile, ConfigNode, logger));
	}

	bool parse_config(std::ifstream &file, ConfigNode &parent, Logger& logger) {
		std::string line;
		int line_nb = parent.lineNumber;

		while (std::getline(file, line)) {
			++line_nb;
			std::string clean = preProcess(line);

			if (clean.empty())
				continue;

			if (isBlockStart(clean)) {
				std::string trimmed = su::rtrim(clean.substr(0, clean.size() - 1));
				std::vector<std::string> tokens = tokenize(trimmed);
				if (tokens.empty()) {
					logger.logWithPrefix(Logger::ERROR, "Configuration file", "Empty block at line " + su::to_string(line_nb));
					return false;
				}
				ConfigNode block;
				block.name = tokens[0];
				block.args = std::vector<std::string>(tokens.begin() + 1, tokens.end());
				block.lineNumber = line_nb;
				if (!ConfigParsing::parse_config(file, block, logger)) {
					return false;
				}
				parent.children.push_back(block);
				continue;
			}

			if (isBlockEnd(clean)) {
				parent.lineNumber = line_nb;
				return true;
			}

			if (isDirective(clean)) {
				std::string trimmed = su::rtrim(clean.substr(0, clean.size() - 1)); // remove ;
				std::vector<std::string> tokens = tokenize(trimmed);
				if (tokens.empty()) {
					logger.logWithPrefix(Logger::ERROR, "Configuration file", "Empty directive at line " + su::to_string(line_nb));
					return false;
				}
				ConfigNode directive;
				directive.name = tokens[0];
				directive.args = std::vector<std::string>(tokens.begin() + 1, tokens.end());
				directive.lineNumber = line_nb;
				parent.children.push_back(directive);
				continue;
			}

			logger.logWithPrefix(Logger::ERROR, "Configuration file", 
				"Unexpected line at " + su::to_string(line_nb) + ": " + clean);
			return false;
		}

		return true;
	}


	//////////////////////////////////////////////
	// Remove comments and trim
	std::string preProcess(const std::string& line) {
		size_t hashpos = line.find('#');
		std::string newline = (hashpos != std::string::npos) ? line.substr(0, hashpos) : line;
		return (su::trim(newline));
	}

	//////////////////////////////////////////////
	// isUtil
	bool isBlockStart(const std::string& line) {
		return (su::ends_with(line, "{"));
	}
	bool isBlockEnd(const std::string& line) {
		return (line == "}") ;
	}
	bool isDirective(const std::string& line) {
		return (su::ends_with(line, ";"));
	}


	//////////////////////////////////////////////
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


	//////////////////////////////////////////////
	// Pretty-print function
	std::string join_args(const std::vector<std::string>& args) {
		
		std::string out = "[";
		for (size_t i = 0; i < args.size(); ++i) {
			out += "\"" + args[i] + "\"";
			if (i + 1 < args.size()) out += ", ";
		}
		out += "]";
		
		return out;
	}
	
	void pretty_print(const ConfigNode& node, const std::string& prefix, bool isLast) {
	
		std::cout << prefix;
		std::cout << (isLast ? "└── " : "├── ");
		std::cout << node.name;
		if (!node.args.empty()) {
			std::cout << " " << join_args(node.args);
		}
		std::cout << std::endl;

		std::string childPrefix = prefix + (isLast ? "    " : "│   ");

		for (size_t i = 0; i < node.children.size(); ++i) {
			bool childIsLast = (i == node.children.size() - 1);
			pretty_print(node.children[i], childPrefix, childIsLast);
		}
	}

}
