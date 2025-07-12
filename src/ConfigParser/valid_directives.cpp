/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   valid_directives.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/22 14:19:50 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/25 17:32:55 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "config_parser.hpp"
#include <cstddef>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

// Helper for multi-context directives (e.g. "server", "location")
std::vector<std::string> make_vec(const std::string& a, const std::string& b) {
	std::vector<std::string> v;
	v.push_back(a);
	v.push_back(b);
	return v;
}

std::vector<Validity> validDirective;

void initValidDirectives() {
	// Format: name, contexts, allowDuplicate, minArgs, maxArgs, validationFunction
	validDirective.push_back(Validity("events", std::vector<std::string>(1, "main"), false, 0, 0, NULL));
	validDirective.push_back(Validity("http", std::vector<std::string>(1, "main"), false, 0, 0, NULL));
	validDirective.push_back(Validity("server", std::vector<std::string>(1, "http"), true, 0, 0, NULL));
	validDirective.push_back(Validity("location", std::vector<std::string>(1, "server"), true, 1, 1, NULL));
	validDirective.push_back(Validity("listen", std::vector<std::string>(1, "server"), false, 1, 1, ConfigParsing::validateListen));
	validDirective.push_back(Validity("server_name", std::vector<std::string>(1, "server"), false, 1, SIZE_MAX, NULL));
	validDirective.push_back(Validity("error_page", make_vec("server", "location"), true, 2, SIZE_MAX, ConfigParsing::validateError));
	validDirective.push_back(Validity("client_max_body_size", make_vec("server", "location"), false, 1, 1, ConfigParsing::validateMaxBody));
	validDirective.push_back(Validity("limit_except", std::vector<std::string>(1, "location"), false, 1, 4, ConfigParsing::validateMethod));
	validDirective.push_back(Validity("return", make_vec("server", "location"), false, 1, 2, NULL));
	validDirective.push_back(Validity("root", make_vec("server", "location"), false, 1, 1, NULL));
	validDirective.push_back(Validity("autoindex", make_vec("server", "location"), false, 1, 1, ConfigParsing::validateAutoIndex));
	validDirective.push_back(Validity("cgi_ext", std::vector<std::string>(1, "location"), false, 1, SIZE_MAX, ConfigParsing::validateExt));
	validDirective.push_back(Validity("index", make_vec("server", "location"), false, 1, SIZE_MAX, NULL));
	validDirective.push_back(Validity("alias", std::vector<std::string>(1, "location"), false, 1, 1, NULL));
	validDirective.push_back(Validity("chunked_transfer_encoding", make_vec("server", "location"), false, 1, 1, ConfigParsing::validateChunk));
}

	// OPTIONAL - OTHER THAT CAN BE INTERESTING
	// {"keepalive_timeout",    {"http", "server"}, false, 1, 2}, // timeout [header_timeout]
	// {"allow",                {"http", "server", "location"}, false, 1, SIZE_MAX},
	// {"deny",                 {"http", "server", "location"}, false, 1, SIZE_MAX},
	// {"error_log",            {"main", "http", "server", "location"}, false, 1, 2}, // file [level]
	// {"access_log",           {"http", "server", "location"}, false, 1, 2},         // file [format]

namespace ConfigParsing {


	bool validateListen(const ConfigNode& node, Logger& logger) {
		std::string value = node.args[0];
		std::string host;
		std::string portStr;

		// handles ":port" 
		if (value[0] == ':')
			portStr = value.substr(1);
		// handles "host:port" 
		else if (value.find(':') != std::string::npos) {
			size_t colonPos = value.find(':');
			host = value.substr(0, colonPos);
			portStr = value.substr(colonPos + 1);
			if (!isValidIPv4(host)) {
				logger.logWithPrefix(Logger::WARNING, "Configuration file", "Invalid IPv4 address in 'listen' directive: " + host);
				return false;
			}
		}
		// handles "port" 
		else 
			portStr = value;

		// validate port (1-65535)
		if (!portStr.empty()) {
			for (size_t i = 0; i < portStr.length(); ++i) {
				if (!isdigit(portStr[i])) {
					logger.logWithPrefix(Logger::WARNING, "Configuration file", "Invalid port in 'listen' directive: " + portStr);
					return false;
				}
			}
			int port = atoi(portStr.c_str());
			if (port < 1 || port > 65535) {
				logger.logWithPrefix(Logger::WARNING, "Configuration file", "Port out of range in 'listen' directive: " + portStr);
				return false;
			}
		}

		return true;
	}

	bool validateError(const ConfigNode& node, Logger& logger) {
		for (size_t i = 0; i < node.args.size() - 1; ++i) {
			std::stringstream ss(node.args[i]);
			int code;
			if (!(ss >> code) || code < 100 || code > 599) {
				logger.logWithPrefix(Logger::WARNING, "Configuration file", "Invalid HTTP error code in 'error_page': " + node.args[i]);
				return false;
			}
		}
		if (node.args.back().empty()) {
			logger.logWithPrefix(Logger::WARNING, "Configuration file", "Missing URI in 'error_page'.");
			return false;
		}
		return true;
	}

	bool validateMaxBody(const ConfigNode& node, Logger& logger) {
		std::istringstream iss(node.args[0]);
		unsigned int n;
		iss >> n;
		if (iss.fail() || !iss.eof()) {
			logger.logWithPrefix(Logger::WARNING, "Configuration file", "client_max_body_size must be a positive integer.");
			return false;
		}
		return true;
	}


	bool validateMethod(const ConfigNode& node, Logger& logger) {
		static const char* valid_methods[] = {"GET", "POST", "DELETE", "HEAD"};
		const int n = 4;

		for (size_t i = 0; i < node.args.size(); ++i) {
			bool found = false;
			for (int j = 0; j < n; ++j) {
				if (node.args[i] == valid_methods[j]) {
					found = true;
					break;
				}
			}
			if (!found) {
				logger.logWithPrefix(Logger::WARNING, "Configuration file",
					"limit_except: unsupported method '" + node.args[i] + "'.");
				return false;
			}
		}
		return true;
	}


	bool validateAutoIndex(const ConfigNode& node, Logger& logger) {
		if (node.args[0] != "on" && node.args[0] != "off") {
			logger.logWithPrefix(Logger::WARNING, "Configuration file", "autoindex must be 'on' or 'off'.");
			return false;
		}
		return true;
	}

	bool validateExt(const ConfigNode& node, Logger& logger) {
		for (size_t i = 0; i < node.args.size(); ++i) {
			if (node.args[i] != ".py" && node.args[i] != ".php") {
				logger.logWithPrefix(Logger::WARNING, "Configuration file", "cgi_ext: unsupported extension '" + node.args[i] + "'.");
				return false;
			}
		}
		return true;
	}

	bool validateChunk(const ConfigNode& node, Logger& logger) {
		if (node.args[0] != "on" && node.args[0] != "off") {
			logger.logWithPrefix(Logger::WARNING, "Configuration file", "chunked_transfer_encoding must be 'on' or 'off'.");
			return false;
		}
		return true;
	}


	bool validateDirective(ConfigNode& childNode, ConfigNode& parent, Logger& logger) {
		// Initialize valid directives if not already done
		if (validDirective.empty()) {
			initValidDirectives();
		}

		for (std::vector<Validity>::const_iterator dir = validDirective.begin(); dir != validDirective.end(); ++dir) {
			if (childNode.name == dir->name) {
				bool contextOK = false;
				for (size_t i = 0; i < dir->contexts.size(); ++i) {
					if (parent.name == dir->contexts[i]) {
						contextOK = true;
						break;
					}
				}
				if (!contextOK) {
					logger.logWithPrefix(Logger::WARNING, "Configuration file", "Directive '" + childNode.name + "' is not allowed in context '" + parent.name + "'.");
					return false;
				}
				if (childNode.args.size() < dir->minArgs || childNode.args.size() > dir->maxArgs) {
					logger.logWithPrefix(Logger::WARNING, "Configuration file", "Directive '" + childNode.name + "' expects between " +
						su::to_string(dir->minArgs) + " and " + su::to_string(dir->maxArgs) + " arguments, but got " + su::to_string(childNode.args.size()) + ".");
					return false;
				}
				if (dir->validF != NULL) {
					if (!dir->validF(childNode, logger))
						return false;
				}
				return true;
			}
		}
		logger.logWithPrefix(Logger::WARNING, "Configuration file", "Unknown directive: '" + childNode.name + "'");
		return false;
	}

}