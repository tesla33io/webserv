
#include "config_parser.hpp"


bool ConfigParser::loadConfig(const std::string& filePath, std::vector<ServerConfig>& servers) {

	ConfigNode tree;
	ConfigParser configparser;

	if (!configparser.parseTree(filePath, tree))
		return false;

	if (!configparser.convertTreeToStruct(tree, servers))
		return false;

	return true;
}


bool ConfigParser::parseTree(const std::string &filePath, ConfigNode &childNode) {
	
	std::ifstream confFile(filePath.c_str());
	if (!confFile.is_open()) {
		logg_.logWithPrefix(Logger::WARNING, "CONFIG", "Could not open file: " + filePath);
		return false;
	}
	childNode.line_ = 0;
	childNode.name_ = "main";
	int line_nb = 0;
	if (!parseTreeBlocks(confFile, line_nb, childNode)) {
		logg_.logWithPrefix(Logger::ERROR, "CONFIG", "Failed to parse configuration.");
		return false;
	}

	logg_.logWithPrefix(Logger::INFO, "CONFIG", "Configuration tree successfully created.");
	logg_.logWithPrefix(Logger::DEBUG, "CONFIG", "Dumping server tree:");
	std::ostringstream oss;
	ConfigParser::printTree(childNode, "", true, oss);
	logg_.logWithPrefix(Logger::DEBUG, "CONFIG", oss.str());
	return true;
}

bool ConfigParser::parseTreeBlocks(std::ifstream &file, int &line_nb, ConfigNode &parent) {

	std::string accumulated_line;
	std::string line;
	int statement_start_line = 0;

	while (std::getline(file, line)) {

		++line_nb;
		std::string clean = ConfigParser::preProcess(line); // Remove comments and trim
		if (clean.empty())
			continue;

		// Traitement des lignes structurelles (blocs)
		if (clean.find('{') != std::string::npos || clean.find('}') != std::string::npos) {
			if (!accumulated_line.empty())
				accumulated_line += " " + clean;
			else {
				accumulated_line = clean;
				statement_start_line = line_nb;
			}
		}
		// Traitement des directives simples (se terminant par ;)
		else if (!clean.empty() && clean[clean.size() - 1] == ';') {
			accumulated_line = clean;
			statement_start_line = line_nb;
		}
		// Ligne non reconnue (directive sans ;)
		else {
			logg_.logWithPrefix(Logger::ERROR, "CONFIG",
			                    "Missing ';' at end of directive at line " +
			                        su::to_string(line_nb) + ": \"" + clean + "\"");
			return false;
		}

		

		std::string statement = accumulated_line;
		accumulated_line.clear();

		if (ConfigParser::isBlockStart(statement)) {
			std::string trimmed = su::rtrim(statement.substr(0, statement.size() - 1));
			std::vector<std::string> tokens = ConfigParser::tokenize(trimmed);
			if (tokens.empty()) {
				logg_.logWithPrefix(Logger::ERROR, "CONFIG",
				                     "Empty ConfigNode at line " +
				                         su::to_string(statement_start_line));
				return false;
			}
			ConfigNode childNode(tokens[0], std::vector<std::string>(tokens.begin() + 1, tokens.end()),
					statement_start_line);

			if (!ConfigParser::validateDirective(childNode, parent))
				return false;

			if (!parseTreeBlocks(file, line_nb, childNode)) {
				return false;
			}
			parent.children_.push_back(childNode);
			continue;
		}

		if (ConfigParser::isBlockEnd(statement)) {
			return true;
		}

		if (ConfigParser::isDirective(statement)) {
			std::string trimmed = su::rtrim(statement.substr(0, statement.size() - 1));
			std::vector<std::string> tokens = ConfigParser::tokenize(trimmed);
			if (tokens.empty()) {
				logg_.logWithPrefix(Logger::ERROR, "CONFIG",
				                     "Empty directive at line " +
				                         su::to_string(statement_start_line));
				return false;
			}
			ConfigNode directive (tokens[0], std::vector<std::string>(tokens.begin() + 1, tokens.end()),
					statement_start_line);
			if (!ConfigParser::validateDirective(directive, parent))
				return false;
			parent.children_.push_back(directive);
			continue;
		}

		logg_.logWithPrefix(Logger::ERROR, "CONFIG",
		                     "Unexpected line at " + su::to_string(statement_start_line) + ": " +
		                         statement);
		return false;
	}

	if (!accumulated_line.empty()) { // last line no closing statement
		logg_.logWithPrefix(Logger::ERROR, "CONFIG",
		                     "Incomplete statement at line " + su::to_string(statement_start_line) +
		                         ": " + accumulated_line);
		return false;
	}

	if (parent.name_ != "main") {
		logg_.logWithPrefix(Logger::ERROR, "CONFIG",
		                     "Unexpected end of file: missing closing bracket for block '" +
		                         parent.name_ + "'");
		return false;
	}

	return true;
}
