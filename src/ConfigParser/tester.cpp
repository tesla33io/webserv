/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tester.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/22 15:58:38 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/23 19:48:13 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config_parser.hpp"
#include "../Logger/Logger.hpp"


int main(int argc, char **argv) {

	Logger logger("config.log", Logger::DEBUG, true);
	ConfigNode config;
	
	if (argc != 2)
		return (1);
		
	if (!ConfigParsing::parser(argv[1], config))
		return (1);

	ConfigParsing::pretty_print(config, "", true);

}

