/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ValidDirective.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/22 11:29:26 by htharrau          #+#    #+#             */
/*   Updated: 2025/07/22 18:59:24 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */



#include "ConfigParser.hpp"

// Constructor - initialize valid directives
ConfigParser::ConfigParser() {
	initValidDirectives();
}

void ConfigParser::initValidDirectives() {
	validDirectives_.clear();
	// above server
	validDirectives_.push_back(Validity("events", std::vector<std::string>(1, "main"), false, 0, 0, NULL));
	validDirectives_.push_back(Validity("http", std::vector<std::string>(1, "main"), true, 0, 0, NULL));
	validDirectives_.push_back(Validity("server", std::vector<std::string>(1, "http"), true, 0, 0, NULL));
	// server only level
	validDirectives_.push_back(Validity("listen", std::vector<std::string>(1, "server"), false, 1, 1, &ConfigParser::validateListen));
	validDirectives_.push_back(Validity("error_page", std::vector<std::string>(1, "server"), true, 2, SIZE_MAX, &ConfigParser::validateError));
	validDirectives_.push_back(Validity("client_max_body_size", std::vector<std::string>(1, "server"), false, 1, 1, &ConfigParser::validateMaxBody));
	validDirectives_.push_back(Validity("location",std::vector<std::string>(1, "server"), true, 1, 2, NULL));
	// server or location level  (will be inherited in the locations if not set in the location)
	validDirectives_.push_back(Validity("root", makeVector("server", "location"), false, 1, 1, NULL));
	validDirectives_.push_back(Validity("allowed_methods", makeVector("server", "location"), false, 1, 3, &ConfigParser::validateMethod));
	validDirectives_.push_back(Validity("upload_path", makeVector("server", "location"), false, 1, 1, NULL));
	validDirectives_.push_back(Validity("autoindex",  makeVector("server", "location"), false, 1, 2, &ConfigParser::validateAutoIndex));
	validDirectives_.push_back(Validity("cgi_ext", makeVector("server", "location"), false, 2, SIZE_MAX, &ConfigParser::validateCGI));
	// location only level
	validDirectives_.push_back(Validity("index", std::vector<std::string>(1, "location"), false, 1, 1, NULL));
	validDirectives_.push_back(Validity("return", std::vector<std::string>(1, "location"), false, 1, 2, &ConfigParser::validateReturn));
	validDirectives_.push_back(Validity("alias", std::vector<std::string>(1, "location"),  false, 1, 1, NULL));
}


bool ConfigParser::validateListen(const ConfigNode& node)  {
	std::string value = node.args_[0];
	std::string host;
	std::string portStr;

	// handles ":port" 
	if (value[0] == ':') {
		portStr = value.substr(1);
	}
	// handles "host:port" 
	else if (value.find(':') != std::string::npos) {
		size_t colonPos = value.find(':');
		host = value.substr(0, colonPos);
		portStr = value.substr(colonPos + 1);
		if (!isValidIPv4(host)) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Invalid IPv4 address in 'listen' directive: " + host);
			return false;
		}
	}
	// handles "port" 
	else {
		portStr = value;
	}

	// validate port (1-65535)
	if (!portStr.empty()) {
		for (size_t i = 0; i < portStr.length(); ++i) {
			if (!isdigit(portStr[i])) {
				logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Invalid port in 'listen' directive: " + portStr);
				return false;
			}
		}
		int port = atoi(portStr.c_str());
		if (port < 1 || port > 65535) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Port out of range in 'listen' directive: " + portStr);
			return false;
		}
	}

	return true;
}

bool ConfigParser::validateReturn(const ConfigNode& node)  {
	std::istringstream ss(node.args_[0]);
	unsigned int code;
	if (!(ss >> code) || code < 100 || code > 599) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Invalid return status code: " + node.args_[0]);
		return false;
	}
	if (code >= 300 && code < 400) {
		if (node.args_.size() != 2) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file", "return code " + su::to_string(code) + " must include a URI or URL.");
			return false;
		}
	} 
	else {
		if (node.args_.size() != 1) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file", "return code " + su::to_string(code) + " must NOT include a URI or URL.");
			return false;
		}
	}
	return true;
}

bool ConfigParser::validateError(const ConfigNode& node)  {
	for (size_t i = 0; i < node.args_.size() - 1; ++i) {
		std::stringstream ss(node.args_[i]);
		int code;
		if (!(ss >> code) || code < 400 || code > 599) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Error page status code must be 400-599. Received: " + node.args_[i]);
			return false;
		}
	}
	if (node.args_.back().empty()) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Missing URI in 'error_page'.");
		return false;
	}
	return true;
}

bool ConfigParser::validateMaxBody(const ConfigNode& node)  {
	std::istringstream iss(node.args_[0]);
	unsigned int n;
	iss >> n;
	if (iss.fail() || !iss.eof()) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file", "client_max_body_size must be a positive integer.");
		return false;
	}
	return true;
}

bool ConfigParser::validateMethod(const ConfigNode& node)  {
	static const char* valid_methods[] = {"GET", "POST", "DELETE"};
	const int n = sizeof(valid_methods) / sizeof(valid_methods[0]);


	for (size_t i = 0; i < node.args_.size(); ++i) {
		bool found = false;
		for (int j = 0; j < n; ++j) {
			if (node.args_[i] == valid_methods[j]) {
				found = true;
				break;
			}
		}
		if (!found) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file",
				"allowed_methods: unsupported method '" + node.args_[i] + "'.");
			return false;
		}
	}
	return true;
}

bool ConfigParser::validateAutoIndex(const ConfigNode& node)  {
	if (node.args_[0] != "on" && node.args_[0] != "off") {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file", "autoindex must be 'on' or 'off'.");
		return false;
	}
	return true;
}

bool ConfigParser::validateCGI(const ConfigNode& node) {
	
	// CGI expects pairs: extension interpreter_path extension interpreter_path 
	if (node.args_.size() % 2 != 0) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file", 
			"cgi_ext expects pairs of extension and interpreter path. Got " + 
			su::to_string(node.args_.size()) + " arguments.");
		return false;
	}
	
	// allowed CGI extensions 
	static const char* allowed_extensions[] = {".py", ".php"};
	const int num_allowed = sizeof(allowed_extensions) / sizeof(allowed_extensions[0]);
	
	// Validate each extension-interpreter pair
	for (size_t i = 0; i < node.args_.size(); i += 2) {
		const std::string& extension = node.args_[i];
		const std::string& interpreter = node.args_[i + 1];	
		// extension in the list
		bool found = false;
		for (int j = 0; j < num_allowed; ++j) {
			if (extension == allowed_extensions[j]) {
				found = true;
				break;
			}
		}
		if (!found) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file", 
				"cgi_ext: unsupported extension '" + extension + "'.");
			return false;
		}
		
		// interpreter path not empty
		if (interpreter.empty()) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file", 
				"cgi_ext: interpreter path cannot be empty for extension '" + extension + "'.");
			return false;
		}
	}
	
	return true;
}


bool ConfigParser::validateDirective(const ConfigNode& node, const ConfigNode& parent)  {

	for (std::vector<Validity>::const_iterator it = validDirectives_.begin(); it != validDirectives_.end(); ++it) {
		if ( it->name_ == node.name_ ) {
			bool contextOK = false;
			for (size_t i = 0; i < it->contexts_.size(); ++i) {
				if (parent.name_ == it->contexts_[i]) {
					contextOK = true;
					break;
				}
			}
			if (!contextOK) {
				logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Directive '" + node.name_ + "' is not allowed in context '" + parent.name_ + "'.");
				return false;
			}
			if (!it->repeatOK_) {
				int count = 0;
				for (size_t i = 0; i < parent.children_.size(); ++i) {
					if (parent.children_[i].name_ == node.name_) {
						count++;
					}
				}
				if (count > 1) {
					logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Directive '" + node.name_ + "' cannot be repeated in context '" + parent.name_ + "'.");
					return false;
				}
			}
			if (node.args_.size() < it->minArgs_ || node.args_.size() > it->maxArgs_) {
				logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Directive '" + node.name_ + "' expects between " +
					su::to_string(it->minArgs_) + " and " + su::to_string(it->maxArgs_) + " arguments, but got " + su::to_string(node.args_.size()) + ".");
				return false;
			}
			if (it->validF_ != NULL) {
				if (!(this->*(it->validF_))(node))
					return false;
			}
			return true;
		}
	}
	logg_.logWithPrefix(Logger::WARNING, "Configuration file", "Unknown directive: '" + node.name_ + "'");
	return false;
}
