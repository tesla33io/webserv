/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ValidDirective.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 13:52:39 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/21 15:50:37 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"

// Constructor - initialize valid directives
ConfigParser::ConfigParser(int log_level)
    : logg_("Config.log",
            log_level == 0 ? Logger::ERROR
                           : (log_level == 1     ? Logger::WARNING
                              : (log_level == 2) ? Logger::INFO
                                                 : Logger::DEBUG),
            true) {
	initValidDirectives();
}

void ConfigParser::initValidDirectives() {
	validDirectives_.clear();
	// above server
	validDirectives_.push_back(
	    Validity("events", std::vector<std::string>(1, "main"), false, 0, 0, NULL));
	validDirectives_.push_back(
	    Validity("http", std::vector<std::string>(1, "main"), true, 0, 0, NULL));
	validDirectives_.push_back(
	    Validity("server", std::vector<std::string>(1, "http"), true, 0, 0, NULL));
	// server only level
	validDirectives_.push_back(Validity("listen", std::vector<std::string>(1, "server"), false, 1,
	                                    1, &ConfigParser::validateListen));
	validDirectives_.push_back(Validity("error_page", std::vector<std::string>(1, "server"), true,
	                                    2, SIZE_MAX, &ConfigParser::validateError));
	validDirectives_.push_back(Validity("client_max_body_size", makeVector("server", "location"),
	                                    false, 1, 1, &ConfigParser::validateMaxBody));
	validDirectives_.push_back(Validity("location", std::vector<std::string>(1, "server"), true, 1,
	                                    1, &ConfigParser::validateLocation));
	// server or location level  (will be inherited in the locations if not set in the location)
	validDirectives_.push_back(Validity("root", makeVector("server", "location"), false, 1, 1,
	                                    &ConfigParser::validateRoot));
	validDirectives_.push_back(Validity("allowed_methods", makeVector("server", "location"), false,
	                                    1, 3, &ConfigParser::validateMethod));
	validDirectives_.push_back(Validity("upload_path", makeVector("server", "location"), false, 1,
	                                    1, &ConfigParser::validateUploadPath));
	validDirectives_.push_back(Validity("cgi_ext", makeVector("server", "location"), false, 2,
	                                    SIZE_MAX, &ConfigParser::validateCGI));
	validDirectives_.push_back(Validity("index", makeVector("server", "location"), false, 1, 1,
	                                    &ConfigParser::validateIndex));
	// location only level
	validDirectives_.push_back(Validity("autoindex", std::vector<std::string>(1, "location"), false,
	                                    1, 1, &ConfigParser::validateAutoIndex));
	validDirectives_.push_back(Validity("return", std::vector<std::string>(1, "location"), false, 1,
	                                    2, &ConfigParser::validateReturn));
}

// CHECK NB OF ARGS, CONTEXT, DUPLICATES, TAILORED VALIDITY FUNCTION
bool ConfigParser::validateDirective(const ConfigNode &node, const ConfigNode &parent) {

	for (std::vector<Validity>::const_iterator it = validDirectives_.begin();
	     it != validDirectives_.end(); ++it) {
		if (it->name_ == node.name_) {
			bool contextOK = false;
			for (size_t i = 0; i < it->contexts_.size(); ++i) {
				if (parent.name_ == it->contexts_[i]) {
					contextOK = true;
					break;
				}
			}
			if (!contextOK) {
				logg_.logWithPrefix(Logger::WARNING, "Configuration file",
				                    "Directive '" + node.name_ + "' is not allowed in context '" +
				                        parent.name_ + "' on line " + su::to_string(node.line_));
				return false;
			}
			if (!it->repeatOK_) {
				int count = 0;
				for (size_t i = 0; i < parent.children_.size(); ++i) {
					if (parent.children_[i].name_ == node.name_) {
						count++;
					}
				}
				if (count > 0) {
					logg_.logWithPrefix(Logger::WARNING, "Configuration file",
					                    "Directive '" + node.name_ +
					                        "' cannot be repeated in context '" + parent.name_ +
					                        "' on line " + su::to_string(node.line_));
					return false;
				}
			}
			if (node.args_.size() < it->min_args_ || node.args_.size() > it->max_args_) {
				logg_.logWithPrefix(Logger::WARNING, "Configuration file",
				                    "Directive '" + node.name_ + "' expects between " +
				                        su::to_string(it->min_args_) + " and " +
				                        su::to_string(it->max_args_) + " arguments, but got " +
				                        su::to_string(node.args_.size()) + " on line " +
				                        su::to_string(node.line_));
				return false;
			}
			if (it->valid_f_ != NULL) {
				if (!(this->*(it->valid_f_))(node))
					return false;
			}
			return true;
		}
	}
	logg_.logWithPrefix(Logger::WARNING, "Configuration file",
	                    "Unknown directive: '" + node.name_ + "' on line " +
	                        su::to_string(node.line_));
	return false;
}

// LISTEN: ipv4:port, or :port, or port ()
bool ConfigParser::validateListen(const ConfigNode &node) {
	std::string value = node.args_[0];
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
			logg_.logWithPrefix(Logger::WARNING, "Configuration file",
			                    "Invalid IPv4 address in 'listen' directive: " + host +
			                        " on line " + su::to_string(node.line_));
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
				logg_.logWithPrefix(Logger::WARNING, "Configuration file",
				                    "Invalid port in 'listen' directive: " + portStr + " on line " +
				                        su::to_string(node.line_));
				return false;
			}
		}
		int port = atoi(portStr.c_str());
		if (port < 1 || port > 65535) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file",
			                    "Port out of range in 'listen' directive: " + portStr +
			                        " on line " + su::to_string(node.line_));
			return false;
		}
	}

	return true;
}

// RETURN : 300 - 599. If no code-> one arg (URL or error), otherwise code + uri/url
bool ConfigParser::validateReturn(const ConfigNode &node) {
	// 1 arg: url or error code
	if (node.args_.size() == 1) {
		const std::string &arg = node.args_[0];
		if (isHttp(arg) || isValidUri(arg))
			return true;
		std::istringstream ss(arg);
		uint16_t code;
		if ((ss >> code) && ss.eof() && code > 399 && !unknownCode(code)) {
			return true;
		}
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "Invalid return value: " + arg + " on line " +
		                        su::to_string(node.line_));
		return false;
	}

	// 2 args : return code + uri/url
	if (node.args_.size() == 2) {
		std::istringstream ss(node.args_[0]);
		uint16_t code;
		if (!(ss >> code) || !ss.eof() || code < 300 || code > 399 || unknownCode(code)) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file",
			                    "Invalid return status code: " + node.args_[0] + " on line " +
			                        su::to_string(node.line_));
			return false;
		}
		// Second must be URI or URL
		if (!isValidUri(node.args_[1]) && !isHttp(node.args_[1])) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file",
			                    "Invalid redirection URI or URL: " + node.args_[1] + " on line " +
			                        su::to_string(node.line_));
			return false;
		}
		return true;
	}

	// --- Any other arg count is invalid ---
	logg_.logWithPrefix(Logger::WARNING, "Configuration file",
	                    "Invalid number of arguments for return on line " +
	                        su::to_string(node.line_));
	return false;
}

// ERROR: 400 - 599, last arg is file
bool ConfigParser::validateError(const ConfigNode &node) {
	for (size_t i = 0; i < node.args_.size() - 1; ++i) {
		std::istringstream iss(node.args_[i]);
		uint16_t code;
		if (!(iss >> code) || iss.fail() || !iss.eof() || code < 400 || code > 599 ||
		    unknownCode(code)) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file",
			                    "Error page status code must exist (400-599). Received: " +
			                        node.args_[i] + " on line " + su::to_string(node.line_));
			return false;
		}
	}
	return true;
}

// in bits - M, K ou G at the end accepted
bool ConfigParser::validateMaxBody(const ConfigNode &node) {
	std::string maxBody = node.args_[0];
	if (maxBody.empty()) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "client_max_body_size cannot be empty on line " +
		                        su::to_string(node.line_));
		return false;
	}
	char last = maxBody[maxBody.size() - 1];
	if (std::tolower(last) == 'k' || std::tolower(last) == 'm' || std::tolower(last) == 'g')
		maxBody = su::rtrim(maxBody.substr(0, maxBody.size() - 1));
	if (maxBody.empty()) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "client_max_body_size invalid format: '" + node.args_[0] +
		                        "' on line " + su::to_string(node.line_));
		return false;
	}
	std::istringstream iss(maxBody);
	unsigned int n;
	if (!(iss >> n) || iss.fail() || !iss.eof()) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "client_max_body_size is invalid: '" + node.args_[0] + "' on line " +
		                        su::to_string(node.line_));
		return false;
	}
	return true;
}

// the path must start with /, ends with /, no invalid char
// duplicates path not allowed
bool ConfigParser::validateLocation(const ConfigNode &node) {
	const std::string &path = node.args_[0];
	if (path.empty() || !isValidUri(path)) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "Invalid location path: " + path + " on line " +
		                        su::to_string(node.line_));
		return false;
	}
	return true;
}

// root: starts with /
bool ConfigParser::validateRoot(const ConfigNode &node) {
	if (node.args_[0].empty() || !isValidUri(node.args_[0])) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "Invalid root : " + node.args_[0] + " on line " +
		                        su::to_string(node.line_));
		return false;
	}
	return true;
}

// upload path : starts with /, // to double check
bool ConfigParser::validateUploadPath(const ConfigNode &node) {
	if (node.args_[0].empty() || !isValidUri(node.args_[0])) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "Invalid upload path : " + node.args_[0] + " on line " +
		                        su::to_string(node.line_));
		return false;
	}
	return true;
}

// must be a file: no weird char, has extension, does not start with /
bool ConfigParser::validateIndex(const ConfigNode &node) {
	if (node.args_[0].empty() || !hasOKChar(node.args_[0]) || node.args_[0][0] == '/') {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "Invalid index : " + node.args_[0] + " on line " +
		                        su::to_string(node.line_));
		return false;
	}
	return true;
}

// must be in the list
bool ConfigParser::validateMethod(const ConfigNode &node) {
	static const char *valid_methods[] = {"GET", "POST", "DELETE"};
	const int n = 3;

	for (size_t i = 0; i < node.args_.size(); ++i) {
		bool found = false;
		for (int j = 0; j < n; ++j) {
			if (node.args_[i] == valid_methods[j]) {
				found = true;
				break;
			}
		}
		if (!found) {
			logg_.logWithPrefix(Logger::ERROR, "Configuration file",
			                    "allowed_methods: unsupported method '" + node.args_[i] +
			                        " on line " + su::to_string(node.line_));
			return false;
		}
	}
	return true;
}

bool ConfigParser::validateAutoIndex(const ConfigNode &node) {
	if (node.args_[0] != "on" && node.args_[0] != "off") {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "autoindex must be 'on' or 'off'. Value " + node.args_[0] +
		                        " on line " + su::to_string(node.line_));
		return false;
	}
	return true;
}

bool ConfigParser::validateCGI(const ConfigNode &node) {

	// CGI expects pairs: extension interpreter_path extension interpreter_path
	if (node.args_.size() % 2 != 0) {
		logg_.logWithPrefix(Logger::WARNING, "Configuration file",
		                    "cgi_ext expects pairs of extension and interpreter path. Got " +
		                        su::to_string(node.args_.size()) + " arguments on line " +
		                        su::to_string(node.line_));
		return false;
	}

	// allowed CGI extensions
	static const char *allowed_extensions[] = {".py", ".php"};
	const int num_allowed = sizeof(allowed_extensions) / sizeof(allowed_extensions[0]);

	// Validate each extension-interpreter pair
	for (size_t i = 0; i < node.args_.size(); i += 2) {
		const std::string &extension = node.args_[i];
		const std::string &interpreter = node.args_[i + 1];
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
			                    "cgi_ext: unsupported extension '" + extension + "' on line " +
			                        su::to_string(node.line_));
			return false;
		}

		// interpreter path not empty
		if (interpreter.empty()) {
			logg_.logWithPrefix(Logger::WARNING, "Configuration file",
			                    "cgi_ext: interpreter path cannot be empty for extension '" +
			                        extension + "' on line " + su::to_string(node.line_));
			return false;
		}
	}
	return true;
}
