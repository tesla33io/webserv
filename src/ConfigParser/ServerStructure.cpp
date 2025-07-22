/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerStructure.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/22 12:16:27 by htharrau          #+#    #+#             */
/*   Updated: 2025/07/22 18:48:44 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"

void ConfigParser::convertTreeToStruct(const ConfigNode &tree, std::vector<ServerConfig> &servers) {
	
	for (std::vector<ConfigNode>::const_iterator node = tree.children_.begin(); node != tree.children_.end(); ++node) {
		
		if (node->name_ == "http") 
			convertTreeToStruct(*node, servers);

		else if (node->name_ == "server") {
			
			ServerConfig server;
			LocConfig forInheritance;

			for (std::vector<ConfigNode>::const_iterator child = node->children_.begin(); child != node->children_.end(); ++child) {
				
				if (child->name_ == "listen")
					handleListen(*child, server);
				else if (child->name_ == "error_page") 
					handleErrorPage(*child, server) ;
				else if (child->name_ == "client_max_body_size") 
					handleBodySize(*child, server) ;
					
				else if (child->name_ == "location") {
					LocConfig location;
					location.path = child->args_[0];
					handleLocationBlock(*child, location);
					server.locations.push_back(location);
				}
				
				else 
					handleForInherit(*child, forInheritance);
			}

			inheritGeneralConfig(server, forInheritance);

			logg_.logWithPrefix(Logger::INFO, "Config parsing", 
				"Parsed server block on " + server.host + ":" + su::to_string(server.port) + 
				" with " + su::to_string(server.locations.size()) + " location(s).");

			logg_.logWithPrefix(Logger::DEBUG, "Config parsing", "Dumping server config");
			std::ostringstream oss;
			printServerConfig(server, oss);
			logg_.logWithPrefix(Logger::DEBUG, "Config parsing", oss.str());

			servers.push_back(server);
		}
	}
}



// HOST AND PORT
void ConfigParser::handleListen(const ConfigNode& node, ServerConfig& server) {
	std::string value = node.args_[0];
	if (value[0] == ':') 
		server.port = std::atoi(value.substr(1).c_str());
	else if (value.find(':') != std::string::npos) {
		size_t colonPos = value.find(':');
		server.host = value.substr(0, colonPos);
		server.port = std::atoi(value.substr(colonPos + 1).c_str());
	}
	else 
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
	std::stringstream ss(node.args_[0]);
	ss >> server.client_max_body_size;
}


// Root, Methods, Upload path, autoindex and CGI can be defined server level -> for inheritance
void ConfigParser::handleForInherit(const ConfigNode &node, LocConfig &location) {
	if (node.name_ == "root") 
		location.root = node.args_[0];
	else if (node.name_ == "allowed_methods") 
		location.allowed_methods = node.args_;
	else if (node.name_ == "upload_path") 
			location.upload_path = node.args_[0];
	else if (node.name_ == "cgi_ext") 
		handleCGI(node, location);
	else if (node.name_ == "autoindex")
		location.autoindex = (node.args_[0] == "on");
}


// Apply server-level configs to locations that don't override them
void ConfigParser::inheritGeneralConfig(ServerConfig& server, const LocConfig& forInheritance) {
	
	for (size_t i = 0; i < server.locations.size(); ++i) {
		
		LocConfig& loc = server.locations[i];
		// Inherit root if location doesn't have root or alias
		if (loc.root.empty() && loc.alias.empty())
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
		// Inherit auto index value if not specified
		if (!loc.autoindex && forInheritance.autoindex)
			loc.autoindex = forInheritance.autoindex;
	}
}


// LOCATION-LEVEL DIRECTIVE HANDLERS

void ConfigParser::handleLocationBlock(const ConfigNode &locNode, LocConfig &location) {
	for (std::vector<ConfigNode>::const_iterator node = locNode.children_.begin(); node != locNode.children_.end(); ++node) {
		if (node->name_ == "allowed_methods") 
			location.allowed_methods = node->args_;
		else if (node->name_ == "root") 
			location.root = node->args_[0];
		else if (node->name_ == "alias") 
			location.alias = node->args_[0];
		else if (node->name_ == "autoindex") 
			location.autoindex = (node->args_[0] == "on");
		else if (node->name_ == "index") 
			location.index = node->args_[0];
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
	std::istringstream ss(node.args_[0]);
	unsigned int code;
	ss >> code;
	location.return_code = code;
	if (node.args_.size() == 2)
		location.return_target = node.args_[1];
	else
		location.return_target.clear();
}

// CGI directive
void ConfigParser::handleCGI(const ConfigNode &node, LocConfig &location) {
	for (size_t i = 0; i < node.args_.size(); i += 2) {
			if (i + 1 < node.args_.size()) {
				location.cgi_extensions[node.args_[i]] = node.args_[i + 1];
			}
	}
}





