/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_parser.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/21 14:53:37 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/25 13:31:08 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config_parser.hpp"
#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"

namespace ConfigParsing {

	bool tree_parser(const std::string &filePath, ConfigNode &childNode, Logger &logger) {
		std::ifstream confFile(filePath.c_str());
		if (!confFile.is_open()) {
			logger.logWithPrefix(Logger::WARNING, "Config parsing", "Could not open file: " + filePath);
			return false;
		}
		childNode.lineNumber = 0;
		childNode.name = "main";
		int line_nb = 0;
		if (!ConfigParsing::parse_config(confFile, line_nb, childNode, logger)) {
			logger.logWithPrefix(Logger::ERROR, "Config parsing", "Failed to parse configuration.");
			return false;
		}

		logger.logWithPrefix(Logger::INFO, "Config parsing", "Configuration tree successfully created.");
		logger.logWithPrefix(Logger::DEBUG, "Config parsing", "Dumping server tree");
		std::ostringstream oss;
		ConfigParsing::print_tree_config(childNode, "", true, oss);
		logger.logWithPrefix(Logger::DEBUG, "Config parsing", oss.str());
		return true;
	}

	bool parse_config(std::ifstream &file, int &line_nb, ConfigNode &parent, Logger& logger) {
		
		std::string line;
		while (std::getline(file, line)) {
			++line_nb;
			std::string clean = preProcess(line);

			if (clean.empty())
				continue;

			if (isConfigNodeStart(clean)) {
				std::string trimmed = su::rtrim(clean.substr(0, clean.size() - 1));
				std::vector<std::string> tokens = tokenize(trimmed);
				if (tokens.empty()) {
					logger.logWithPrefix(Logger::ERROR, "Config parsing", "Empty ConfigNode at line " + su::to_string(line_nb));
					return false;
				}
				ConfigNode childNode;
				childNode.name = tokens[0];
				childNode.args = std::vector<std::string>(tokens.begin() + 1, tokens.end());
				childNode.lineNumber = line_nb;
				if (!ConfigParsing::parse_config(file, line_nb, childNode, logger)) {
					logger.logWithPrefix(Logger::ERROR, "Config parsing",
					"Unexpected token or structure at line " + su::to_string(line_nb) + ": " + clean);
					return false;
				}
				parent.children.push_back(childNode);
				continue;
			}

			if (isConfigNodeEnd(clean)) {
				return true;
			}

			if (isDirective(clean)) {
				std::string trimmed = su::rtrim(clean.substr(0, clean.size() - 1)); // remove ;
				std::vector<std::string> tokens = tokenize(trimmed);
				if (tokens.empty()) {
					logger.logWithPrefix(Logger::ERROR, "Config parsing", "Empty directive at line " + su::to_string(line_nb));
					return false;
				}
				ConfigNode directive;
				directive.name = tokens[0];
				directive.args = std::vector<std::string>(tokens.begin() + 1, tokens.end());
				directive.lineNumber = line_nb;
				parent.children.push_back(directive);
				continue;
			}

			logger.logWithPrefix(Logger::ERROR, "Config parsing", "Unexpected line at " + su::to_string(line_nb) + ": " + clean);
			return false;
		}

		if (parent.name != "main") {
			logger.logWithPrefix(Logger::ERROR, "Config parsing",
				"Unexpected end of file: missing closing bracket for block `" + parent.name + "`");
			return false;
		}

		return true;
	}

}
