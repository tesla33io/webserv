/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerStructure.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 13:55:04 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 13:55:14 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"

bool ConfigParser::convertTreeToStruct(const ConfigNode &tree, std::vector<ServerConfig> &servers) {

	for (std::vector<ConfigNode>::const_iterator node = tree.children_.begin();
	     node != tree.children_.end(); ++node) {

		if (node->name_ == "http") {
			if (!convertTreeToStruct(*node, servers))
				return false;
		}

		else if (node->name_ == "server") {

			ServerConfig server;
			LocConfig forInheritance;

			for (std::vector<ConfigNode>::const_iterator child = node->children_.begin();
			     child != node->children_.end(); ++child) {

				if (child->name_ == "listen")
					handleListen(*child, server);
				else if (child->name_ == "error_page")
					handleErrorPage(*child, server);
				else if (child->name_ == "client_max_body_size")
					handleBodySize(*child, server);

				else if (child->name_ == "location") {
					LocConfig location;
					location.path = child->args_[0];
					handleLocationBlock(*child, location);
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
					handleForInherit(*child, forInheritance);
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
			addRootToErrorUri(server);

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
		server.error_pages[code] = uri;
	}
}

// MAX BODY SIZE
void ConfigParser::handleBodySize(const ConfigNode &node, ServerConfig &server) {

	// megabits or giga
	int factor = 1;
	char last = node.args_[0][node.args_[0].size() - 1];
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
	server.client_max_body_size = maxBodyFactor * factor;
}

// Root, Methods, Upload path, autoindex and CGI can be defined server level -> for inheritance
void ConfigParser::handleForInherit(const ConfigNode &node, LocConfig &location) {
	if (node.name_ == "root")
		handleRoot(node, location);
	else if (node.name_ == "allowed_methods")
		location.allowed_methods = node.args_;
	else if (node.name_ == "upload_path")
		location.upload_path = node.args_[0];
	else if (node.name_ == "index")
		location.index = node.args_[0];
	else if (node.name_ == "cgi_ext")
		handleCGI(node, location);
}

////////////////////
// LOCATION-LEVEL DIRECTIVE HANDLERS
////

void ConfigParser::handleLocationBlock(const ConfigNode &locNode, LocConfig &location) {
	for (std::vector<ConfigNode>::const_iterator node = locNode.children_.begin();
	     node != locNode.children_.end(); ++node) {
		if (node->name_ == "allowed_methods")
			location.allowed_methods = node->args_;
		else if (node->name_ == "root")
			handleRoot(*node, location);
		else if (node->name_ == "autoindex")
			location.autoindex = (node->args_[0] == "on");
		else if (node->name_ == "index")
			handleIndex(*node, location);
		else if (node->name_ == "upload_path")
			location.upload_path = node->args_[0];
		else if (node->name_ == "return")
			handleReturn(*node, location);
		else if (node->name_ == "cgi_ext")
			handleCGI(*node, location);
	}
}

// Return directive
void ConfigParser::handleReturn(const ConfigNode &node, LocConfig &location) {
	if (node.args_.size() == 1) {
		location.return_code = 302;
		location.return_target = node.args_[0];
	} else {
		std::istringstream ss(node.args_[0]);
		unsigned int code;
		ss >> code;
		location.return_code = code;
		location.return_target = node.args_[1];
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

// ROOT
void ConfigParser::handleRoot(const ConfigNode &node, LocConfig &location) {
	if (node.args_[0].length() > 1 && su::ends_with(node.args_[0], "/"))
		location.root = node.args_[0].substr(0, node.args_[0].length() - 1);
	else
		location.root = node.args_[0];
}

// Index
void ConfigParser::handleIndex(const ConfigNode &node, LocConfig &location) {
	if (su::starts_with(node.args_[0], "/"))
		location.index = node.args_[0].substr(1, node.args_[0].length());
	else
		location.index = node.args_[0];
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

// ADD root TO ERROR PAGE URI
void ConfigParser::addRootToErrorUri(ServerConfig &server) {
	LocConfig *defaultL = server.defaultLocation();
	if (!server.error_pages.empty() && defaultL != NULL && !defaultL->root.empty()) {
		const std::string &root = defaultL->root;
		for (std::map<uint16_t, std::string>::iterator it = server.error_pages.begin();
		     it != server.error_pages.end(); ++it) {
			const std::string original = it->second;
			it->second = root + ((original[0] == '/') ? "" : "/") + original;
		}
	}
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
