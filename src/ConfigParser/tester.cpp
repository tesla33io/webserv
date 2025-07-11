/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tester.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/22 15:58:38 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/25 13:46:00 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config_parser.hpp"
#include "../Logger/Logger.hpp"


int main(int argc, char **argv) {

	Logger logger("Configuration file", Logger::DEBUG, true);
	if (argc != 2)
		return (1);

	ConfigNode config;
	if (!ConfigParsing::tree_parser(argv[1], config, logger))
		return (1);
	std::vector<ServerConfig> servers;
	ConfigParsing::struct_parser(config, servers, logger);

	return (0);
}

