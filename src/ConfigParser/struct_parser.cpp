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
#include "../Logger/Logger.hpp"
#include "../Utils/StringUtils.hpp"
			
namespace ConfigParsing {

	void struct_parser(const ConfigNode &tree, std::vector<ServerConfig> &servers, Logger &logger) {

		for (std::vector<ConfigNode>::const_iterator node = tree.children.begin(); node != tree.children.end(); ++node) {
		
			if (node->name == "http") 
				struct_parser(*node, servers, logger);

			else if (node->name == "server") {
				ServerConfig	server;
				LocConfig 		general_dir;

				for (std::vector<ConfigNode>::const_iterator child = node->children.begin(); child != node->children.end(); ++child) {
					if (child->name == "host") 
						server.host = child->args[0];
					else if (child->name == "listen") {
						std::stringstream ss(child->args[0]);
						ss >> server.port;
					}
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
		else if (child.name == "allow_methods") 
			location.allow_methods = child.args;
		else if (child.name == "return") 
			location.return_url = child.args[0];
		else if (child.name == "root") 
			location.root = child.args[0];
		else if (child.name == "autoindex") 
			location.autoindex = (child.args[0] == "on");
		else if (child.name == "cgi_path") 
			location.cgi_path = child.args[0];
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

		
	void inherit_gen_dir(ServerConfig &server, const LocConfig &general_dir) {
		for (size_t i = 0; i < server.locations.size(); ++i) {
			LocConfig &loc = server.locations[i];
			if (loc.allow_methods.empty())
				loc.allow_methods = general_dir.allow_methods;
			if (loc.cgi_ext.empty())
				loc.cgi_ext = general_dir.cgi_ext;		
			if (loc.cgi_path.empty())
				loc.cgi_path = general_dir.cgi_path;
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