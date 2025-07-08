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
				ServerConfig server;

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
						handle_location(location, child->children);
						server.locations.push_back (location);
					}
				}
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

				if (child->name == "error_page")
					location.error_pages = handle_error_page(child->args); // map
				else if (child->name == "client_max_body_size") {
					std::stringstream ss(child->args[0]);
					ss >> location.client_max_body_size;
				}
				else if (child->name == "allow_methods") 
					location.allow_methods = child->args;
				else if (child->name == "return") 
					location.return_url = child->args[0];
				else if (child->name == "root") 
					location.root = child->args[0];
				else if (child->name == "autoindex") 
					location.autoindex = (child->args[0] == "on");
				else if (child->name == "cgi_path") 
					location.cgi_path = child->args[0];
				else if (child->name == "cgi_ext") 
					location.cgi_ext = child->args;
				else if (child->name == "index") 
					location.index = child->args;
		}
	}

	std::map<int, std::string> handle_error_page(const std::vector<std::string> &args) {
		std::map<int, std::string> map;
		std::string uri = args.back();
		for (size_t i = 0; i < args.size() - 1; ++i) {
			int code = std::atoi(args[i].c_str());
			map[code] = uri;
		}
		return map;
	}

}