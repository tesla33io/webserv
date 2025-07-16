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

#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"
#include "config_parser.hpp"

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
	if (!ConfigParsing::tree_parser_blocks(confFile, line_nb, childNode, logger)) {
		logger.logWithPrefix(Logger::ERROR, "Config parsing", "Failed to parse configuration.");
		return false;
	}

	logger.logWithPrefix(Logger::INFO, "Config parsing",
	                     "Configuration tree successfully created.");
	logger.logWithPrefix(Logger::DEBUG, "Config parsing", "Dumping server tree");
	std::ostringstream oss;
	ConfigParsing::print_tree_config(childNode, "", true, oss);
	logger.logWithPrefix(Logger::DEBUG, "Config parsing", oss.str());
	return true;
}

bool tree_parser_blocks(std::ifstream &file, int &line_nb, ConfigNode &parent, Logger &logger) {

	std::string accumulated_line;
	std::string line;
	int statement_start_line = 0;

	while (std::getline(file, line)) {

		++line_nb;
		std::string clean = preProcess(line); // Remove comments and trim
		if (clean.empty())
			continue;

		if (accumulated_line.empty())
			statement_start_line = line_nb;
		if (!accumulated_line.empty())
			accumulated_line += " " + clean;
		else
			accumulated_line = clean;

		bool complete_statement = false;
		if (accumulated_line[accumulated_line.size() - 1] == ';' ||
		    accumulated_line[accumulated_line.size() - 1] == '}' ||
		    accumulated_line[accumulated_line.size() - 1] == '{')
			complete_statement = true;
		else
			continue;
		(void)complete_statement;

		std::string statement = accumulated_line;
		accumulated_line.clear();

		if (isConfigNodeStart(statement)) {
			std::string trimmed = su::rtrim(statement.substr(0, statement.size() - 1));
			std::vector<std::string> tokens = tokenize(trimmed);
			if (tokens.empty()) {
				logger.logWithPrefix(Logger::ERROR, "Config parsing",
				                     "Empty ConfigNode at line " +
				                         su::to_string(statement_start_line));
				return false;
			}
			ConfigNode childNode;
			childNode.name = tokens[0];
			childNode.args = std::vector<std::string>(tokens.begin() + 1, tokens.end());
			childNode.lineNumber = statement_start_line;

			if (!ConfigParsing::validateDirective(childNode, parent, logger))
				return false;

			if (!ConfigParsing::tree_parser_blocks(file, line_nb, childNode, logger)) {
				logger.logWithPrefix(Logger::ERROR, "Config parsing",
				                     "Unexpected token or structure at line " +
				                         su::to_string(line_nb) + ": " + statement);
				return false;
			}
			parent.children.push_back(childNode);
			continue;
		}

		if (isConfigNodeEnd(statement)) {
			return true;
		}

		if (isDirective(statement)) {
			std::string trimmed = su::rtrim(statement.substr(0, statement.size() - 1));
			std::vector<std::string> tokens = tokenize(trimmed);
			if (tokens.empty()) {
				logger.logWithPrefix(Logger::ERROR, "Config parsing",
				                     "Empty directive at line " +
				                         su::to_string(statement_start_line));
				return false;
			}
			ConfigNode directive;
			directive.name = tokens[0];
			directive.args = std::vector<std::string>(tokens.begin() + 1, tokens.end());
			directive.lineNumber = statement_start_line;
			if (!ConfigParsing::validateDirective(directive, parent, logger))
				return false;
			parent.children.push_back(directive);
			continue;
		}

		logger.logWithPrefix(Logger::ERROR, "Config parsing",
		                     "Unexpected line at " + su::to_string(statement_start_line) + ": " +
		                         statement);
		return false;
	}

	if (!accumulated_line.empty()) { // last line no closing statement
		logger.logWithPrefix(Logger::ERROR, "Config parsing",
		                     "Incomplete statement at line " + su::to_string(statement_start_line) +
		                         ": " + accumulated_line);
		return false;
	}

	if (parent.name != "main") {
		logger.logWithPrefix(Logger::ERROR, "Config parsing",
		                     "Unexpected end of file: missing closing bracket for block `" +
		                         parent.name + "`");
		return false;
	}

	return true;
}

} // namespace ConfigParsing

