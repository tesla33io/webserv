/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/22 15:58:38 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/22 16:28:52 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"


int main(int argc, char **argv) {

	Logger logger("webserver.log", Logger::DEBUG, true);
	ConfigNode config;
	

	config = parseConfig(argv[1]);
	
	pretty_print;
		

	return (0);
}