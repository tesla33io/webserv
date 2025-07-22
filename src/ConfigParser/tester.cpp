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

#include "ConfigParser.hpp"
#include "../Logger/Logger.hpp"


int main(int argc, char **argv) {

	if (argc != 2)
		return (1);

	ConfigNode tree;
	ConfigParser configparser;
	if (!configparser.parseTree(argv[1], tree))
		return (1);

	std::vector<ServerConfig> servers;
	configparser.convertTreeToStruct(tree, servers);

	return (0);
}
