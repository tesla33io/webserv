/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/21 14:53:33 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/22 16:19:31 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <vector>
#include <string>
#include <limits>
#include <iostream>

#include "../Logger/Logger.hpp"


struct Validity {
	std::string name;
	std::vector<std::string> contexts;
	bool nested;
	size_t maxArgs; 
};


struct ConfigNode {
	std::string name;
	std::vector<std::string> args;
	std::vector<ConfigNode> children;
	int lineNumber;
};

namespace config{

void pretty_print(const ConfigNode& node, const std::string& prefix = "", bool isLast = true);

}

#endif
