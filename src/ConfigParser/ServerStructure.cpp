/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerStructure.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: Invalid date        by                   #+#    #+#             */
/*   Updated: 2025/08/21 15:01:00 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"
#include "Struct.hpp"

bool ConfigParser::convertTreeToStruct(const ConfigNode &tree, std::vector<ServerConfig> &servers, std::string &prefix) {

	for (std::vector<ConfigNode>::const_iterator node = tree.children_.begin();
		 node != tree.children_.end(); ++node) {

		if (node->name_ == "http") {
			if (!convertTreeToStruct(*node, servers, prefix))
				return false;
		}

		else if (node->name_ == "server") {

			ServerConfig server;
			server.prefix_ = prefix;
			LocConfig forInheritance;

			for (std::vector<ConfigNode>::const_iterator child = node->children_.begin();
				 child != node->children_.end(); ++child) {

				if (child->name_ == "listen")
					handleListen(*child, server);
				else if (child->name_ == "error_page")
					handleErrorPage(*child, server);


				else if (child->name_ == "location") {
					LocConfig location;
					location.path = child->args_[0];
					handleLocationBlock(*child, location, server.prefix_);
					// check for duplicates locations
					if (existentLocationDuplicate(server, location)) {
						logg_.logWithPrefix(
							Logger::ERROR, "Configuration file",
							"Location block already exists for this path: " + child->args_[0] +
								", line " + su::to_string(child->line_));
						return false;
					}
					server.locations.push_back(location);
				}

				else
					handleForInherit(*child, forInheritance, server.prefix_);
			}

			// check for duplicate host:port combination
			if (isDuplicateServer(servers, server)) {
				logg_.logWithPrefix(Logger::ERROR, "Configuration file",
									"Duplicate server configuration for " + server.host + ":" +
										su::to_string(server.port));
				return false;
			}

			// create default location "/" if no locations exist or no '/' location exist
			if (server.locations.empty() || !baseLocation(server)) {
				LocConfig defaultLocation;
				defaultLocation.path = "/";
				server.locations.push_back(defaultLocation);
				logg_.logWithPrefix(Logger::DEBUG, "Config parsing",
									"Base/default location block created for" + server.host + ":" +
										su::to_string(server.port));
			}

			inheritGeneralConfig(server, forInheritance);
			sortLocations(server.locations);

			logg_.logWithPrefix(Logger::INFO, "Config parsing",
								"Parsed server block on " + server.host + ":" +
									su::to_string(server.port) + " with " +
									su::to_string(server.locations.size()) + " location(s).");

			logg_.logWithPrefix(Logger::DEBUG, "Config parsing", "Dumping server config");
			std::ostringstream oss;
			printServerConfig(server, oss);
			logg_.logWithPrefix(Logger::DEBUG, "Config parsing", oss.str());

			servers.push_back(server);
		}
	}
	return true;
}


////////////////////
// SERVER-LEVEL DIRECTIVE HANDLERS
////

// HOST AND PORT
void ConfigParser::handleListen(const ConfigNode &node, ServerConfig &server) {
	std::string value = node.args_[0];
	if (value[0] == ':')
		server.port = std::atoi(value.substr(1).c_str());
	else if (value.find(':') != std::string::npos) {
		size_t colonPos = value.find(':');
		server.host = value.substr(0, colonPos);
		server.port = std::atoi(value.substr(colonPos + 1).c_str());
	} else
		server.port = std::atoi(value.c_str());
}

// ERROR PAGES - map code - html
void ConfigParser::handleErrorPage(const ConfigNode &node, ServerConfig &server) {
	std::string uri = node.args_.back();
	for (size_t i = 0; i < node.args_.size() - 1; ++i) {
		int code = std::atoi(node.args_[i].c_str());
		server.error_pages[code] = addPrefix(uri, server.getPrefix());
	}
}

// Root, Methods, Upload path, autoindex, CGI and max body size can be defined server level -> for inheritance
void ConfigParser::handleForInherit(const ConfigNode &node, LocConfig &location, const std::string &prefix) {
	if (node.name_ == "root")
		handleRoot(node, location, prefix);
	else if (node.name_ == "allowed_methods")
		location.allowed_methods = node.args_;
	else if (node.name_ == "upload_path") {
		std::string path = (su::back(node.args_[0]) == '/')? 
							node.args_[0] : node.args_[0] + "/" ;
		location.upload_path = addPrefix(path, prefix);
	}
	else if (node.name_ == "index")
		location.index = node.args_[0];
	else if (node.name_ == "cgi_ext")
		handleCGI(node, location);
	else if (node.name_ == "client_max_body_size")
		handleBodySize(node, location);
}


////////////////////
// LOCATION-LEVEL DIRECTIVE HANDLERS
////

void ConfigParser::handleLocationBlock(const ConfigNode &locNode, LocConfig &location, const std::string &prefix) {
	for (std::vector<ConfigNode>::const_iterator node = locNode.children_.begin();
		 node != locNode.children_.end(); ++node) {
		if (node->name_ == "allowed_methods")
			location.allowed_methods = node->args_;
		else if (node->name_ == "root")
			handleRoot(*node, location, prefix);
		else if (node->name_ == "autoindex")
			location.autoindex = (node->args_[0] == "on");
		else if (node->name_ == "index")
			handleIndex(*node, location);
		else if (node->name_ == "upload_path")
			location.upload_path = addPrefix(node->args_[0], prefix);
		else if (node->name_ == "return")
			handleReturn(*node, location);
		else if (node->name_ == "cgi_ext")
			handleCGI(*node, location);
		else if (node->name_ == "client_max_body_size")
			handleBodySize(*node, location);
	}
}

// ROOT
void ConfigParser::handleRoot(const ConfigNode &node, LocConfig &location, const std::string &prefix){
	
	if (node.args_[0].length() > 1 && su::ends_with(node.args_[0], "/"))
		location.root = addPrefix(node.args_[0].substr(0, node.args_[0].length() - 1), prefix);
	else
		location.root = addPrefix(node.args_[0], prefix);
}

// Index
void ConfigParser::handleIndex(const ConfigNode &node, LocConfig &location) {
	if (su::starts_with(node.args_[0], "/"))
		location.index = node.args_[0].substr(1, node.args_[0].length());
	else
		location.index = node.args_[0];
}

// Return directive
void ConfigParser::handleReturn(const ConfigNode &node, LocConfig &location) {
	std::istringstream ss(node.args_[0]);
	unsigned int code;

	if ((ss >> code) && ss.eof() && code != 0) {
		// Parsed a number successfully
		if (node.args_.size() == 1) {
			location.return_code = code;
			location.return_target = "";
		} else {
			location.return_code = code;
			location.return_target = node.args_[1];
		}
	} else {
		// Not a valid number → treat as URL/URI
		location.return_code = 302;
		location.return_target = node.args_[0];
	}
}

// CGI directive
void ConfigParser::handleCGI(const ConfigNode &node, LocConfig &location) {
	for (size_t i = 0; i < node.args_.size(); i += 2) {
		if (i + 1 < node.args_.size()) {
			location.cgi_extensions[node.args_[i]] = node.args_[i + 1];
		}
	}
}

// MAX BODY SIZE
void ConfigParser::handleBodySize(const ConfigNode &node, LocConfig &location) {
	// megabits or giga
	int factor = 1;
	char last = su::back(node.args_[0]);
	if (std::tolower(last) == 'k')
		factor = 1024;
	else if (std::tolower(last) == 'm')
		factor = 1024 * 1024;
	else if (std::tolower(last) == 'g')
		factor = 1024 * 1024 * 1024;

	std::string maxBody = node.args_[0];
	if (factor > 1)
		maxBody = su::rtrim(maxBody.substr(0, maxBody.size() - 1));

	std::istringstream iss(maxBody);
	size_t maxBodyFactor;
	iss >> maxBodyFactor;
	location.client_max_body_size = maxBodyFactor * factor;
	location.body_size_set = true;
}



////////////////////
// POST CHECKS AND VALIDATION AND MODIFICATION
////

// generate the base location '/' if does not exist
bool ConfigParser::baseLocation(ServerConfig &server) {
	for (size_t i = 0; i < server.locations.size(); ++i) {
		LocConfig loc = server.locations[i];
		if (loc.path == "/")
			return true;
	}
	return false;
}

// Apply server-level configs to locations that don't override them
void ConfigParser::inheritGeneralConfig(ServerConfig &server, const LocConfig &forInheritance) {

	for (size_t i = 0; i < server.locations.size(); ++i) {

		LocConfig &loc = server.locations[i];

		if (loc.root.empty())
			loc.root = forInheritance.root;
		// Inherit methods if location doesn't specify any
		if (loc.allowed_methods.empty())
			loc.allowed_methods = forInheritance.allowed_methods;
		// Inherit upload path if not specified
		if (loc.upload_path.empty())
			loc.upload_path = forInheritance.upload_path;
		// Inherit CGI extensions if not specified
		if (loc.cgi_extensions.empty())
			loc.cgi_extensions = forInheritance.cgi_extensions;
		// Inherit max body size if not specified
		if (forInheritance.body_size_set == true && loc.body_size_set == false) {
			loc.client_max_body_size = forInheritance.client_max_body_size;
			loc.body_size_set = true;
		}
		// Inherit index only in base / default location
		if (loc.path == "/" && loc.index.empty())
			loc.index = forInheritance.index;
	}
}

// HOST:SERVER dupliactes -> not accepted
bool ConfigParser::isDuplicateServer(const std::vector<ServerConfig> &servers,
									 const ServerConfig &newServer) {
	for (std::vector<ServerConfig>::const_iterator it = servers.begin(); it != servers.end();
		 ++it) {
		if (it->host == newServer.host && it->port == newServer.port) {
			return true;
		}
	}
	return false;
}

// LOCATION PATH dupliactes -> not accepted
bool ConfigParser::existentLocationDuplicate(const ServerConfig &server,
											 const LocConfig &location) {
	for (size_t i = 0; i < server.locations.size(); ++i) {
		LocConfig loc = server.locations[i];
		if (loc.path == location.path)
			return true;
	}
	return false;
}


// SORT LOCATIONS by path length (longest first for proper nginx-style matching)
void ConfigParser::sortLocations(std::vector<LocConfig> &locations) {
	std::sort(locations.begin(), locations.end(), compareLocationPaths);
}
bool ConfigParser::compareLocationPaths(const LocConfig &a, const LocConfig &b) {
	if (a.path.length() != b.path.length())
		return a.path.length() > b.path.length();
	return a.path < b.path;
}

std::string ConfigParser::addPrefix(const std::string &uri, const std::string &prefix_) {

	std::string prefix = (!prefix_.empty() && su::back(prefix_) == '/')
								? prefix_.substr(0, prefix_.length() - 1)
								: prefix_;

	std::string resolved_uri;

	if (!uri.empty() && uri[0] == '.') {
		resolved_uri = prefix + uri.substr(1);
	} else {
		resolved_uri = uri;
	}

	if (resolved_uri.empty() || resolved_uri[0] != '/')
		resolved_uri = "/" + resolved_uri;

	return resolved_uri;
}
