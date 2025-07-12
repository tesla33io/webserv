/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   struct_parser.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 13:42:34 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/25 13:42:50 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config_parser.hpp"

	
namespace ConfigParsing {

	void struct_parser(const ConfigNode &tree, std::vector<ServerConfig> &servers, Logger &logger) {

		for (std::vector<ConfigNode>::const_iterator node = tree.children.begin(); node != tree.children.end(); ++node) {
		
			if (node->name == "http") 
				struct_parser(*node, servers, logger);

			else if (node->name == "server") {
				ServerConfig	server;
				LocConfig 		general_dir;

				for (std::vector<ConfigNode>::const_iterator child = node->children.begin(); child != node->children.end(); ++child) {
					if (child->name == "listen")
						ConfigParsing::handle_listen(*child, server.host, server.port);
					else if (child->name == "server_name") 
						server.server_names = child->args;
					else if (child->name == "location") {
						LocConfig location;
						location.path = child->args[0];
						ConfigParsing::handle_location(location, child->children);
						server.locations.push_back(location);
					}
					else {
						ConfigParsing::handle_loc_details(general_dir, *child);
					}
				}

				ConfigParsing::inherit_gen_dir(server, general_dir);

				logger.logWithPrefix(Logger::INFO, "Config parsing", 
  				  "Parsed server block on " + server.host + ":" +  su::to_string(server.port) + 
   				 " with " + su::to_string(server.locations.size()) + " location(s).");

				logger.logWithPrefix(Logger::DEBUG, "Config parsing", "Dumping server config");
				std::ostringstream oss;
				ConfigParsing::print_server_config(server, oss);
				logger.logWithPrefix(Logger::DEBUG, "Config parsing", oss.str());

				servers.push_back(server);
			}
		}
	}

	void handle_location(LocConfig &location, const std::vector<ConfigNode> &children) {
		for (std::vector<ConfigNode>::const_iterator child = children.begin(); child != children.end(); ++child ) {
			ConfigParsing::handle_loc_details(location, *child);
		}
	}
	
	void handle_loc_details(LocConfig &location, const ConfigNode &child) {
		if (child.name == "error_page")
			handle_error_page(location.error_pages, child.args);
		else if (child.name == "client_max_body_size") {
			std::stringstream ss(child.args[0]);
			ss >> location.client_max_body_size;
		}
		else if (child.name == "limit_except") 
			location.limit_except = child.args;
		else if (child.name == "return") 
			location.return_url = child.args[0];
		else if (child.name == "root") 
			location.root = child.args[0];
		else if (child.name == "autoindex") 
			location.autoindex = (child.args[0] == "on");
		else if (child.name == "cgi_ext") 
			location.cgi_ext = child.args;
		else if (child.name == "index") 
			location.index = child.args;
	}
	
	
	void handle_error_page(std::map<int, std::string>& error_pages, const std::vector<std::string>& args) {
		std::string uri = args.back();
		for (size_t i = 0; i < args.size() - 1; ++i) {
			int code = std::atoi(args[i].c_str());
			error_pages[code] = uri;
		}
	}


	void handle_listen(const ConfigNode& node, std::string& host, int& port) {
		std::string value = node.args[0];
		if (value[0] == ':') 
			port = std::atoi(value.substr(1).c_str());
		else if (value.find(':') != std::string::npos) {
			size_t colonPos = value.find(':');
			host = value.substr(0, colonPos);
			port = std::atoi(value.substr(colonPos + 1).c_str());
		}
		else 
			port = std::atoi(value.c_str());
	}

		
	void inherit_gen_dir(ServerConfig &server, const LocConfig &general_dir) {
		for (size_t i = 0; i < server.locations.size(); ++i) {
			LocConfig &loc = server.locations[i];
			if (loc.limit_except.empty())
				loc.limit_except = general_dir.limit_except;
			if (loc.cgi_ext.empty())
				loc.cgi_ext = general_dir.cgi_ext;		
			if (loc.client_max_body_size == 0)
				loc.client_max_body_size = general_dir.client_max_body_size;
			if (loc.return_url.empty())
				loc.return_url = general_dir.return_url;
			if (loc.root.empty())
				loc.root = general_dir.root;		
			if (loc.autoindex == 0)
				loc.autoindex = general_dir.autoindex;
			if (loc.index.empty())
				loc.index = general_dir.index;				
			ConfigParsing::inherit_error_pages(loc.error_pages, general_dir.error_pages);
		}
	}

	void inherit_error_pages(std::map<int, std::string>& loc_map, const std::map<int, std::string>& general_map) {
		for (std::map<int, std::string>::const_iterator it = general_map.begin(); it != general_map.end(); ++it) {
			if (loc_map.find(it->first) == loc_map.end()) {
				loc_map[it->first] = it->second;
			}
		}
	}


}