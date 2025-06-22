/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/22 15:53:58 by htharrau          #+#    #+#             */
/*   Updated: 2025/06/22 16:25:20 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "ConfigParser.hpp"


//////////////////////////////////////////////
// Trimming (from the request parser)
std::string trim(const std::string &line) {
	size_t start = line.find_first_not_of(" \t\r\n");
	size_t end = line.find_last_not_of(" \t\r\n");
	return (start == std::string::npos) ? "" : line.substr(start, end - start + 1);
}


//////////////////////////////////////////////
// Remove comments
std::string rm_comments(const std::string& line) {
	size_t hash = line.find('#');
	std::string clean = (hash == std::string::npos) ? line : line.substr(0, hash);
	return trim(clean);
}


//////////////////////////////////////////////
// Tokens
std::vector<std::string> tokenize(const std::string& line) {
	std::istringstream iss(line);
	std::vector<std::string> tokens;
	std::string token;
	while (iss >> token) {
		tokens.push_back(token);
	}
	return tokens;
}



//////////////////////////////////////////////
// Recursive pretty-print function
std::string join_args(const std::vector<std::string>& args) {
	
	std::string out = "[";
	for (size_t i = 0; i < args.size(); ++i) {
		out += "\"" + args[i] + "\"";
		if (i + 1 < args.size()) out += ", ";
	}
	out += "]";
	
	return out;
}

void pretty_print(const ConfigNode& node, const std::string& prefix = "", bool isLast = true) {
	
	std::cout << prefix;

	if (!prefix.empty()) {
		std::cout << (isLast ? "└── " : "├── ");
	}

	std::cout << node.name;
	if (!node.args.empty()) {
		std::cout << " " << join_args(node.args);
	}
	std::cout << std::endl;

	std::string childPrefix = prefix;
	if (!prefix.empty()) {
		childPrefix += (isLast ? "    " : "│   ");
	}

	for (size_t i = 0; i < node.children.size(); ++i) {
		pretty_print(node.children[i], childPrefix, i + 1 == node.children.size());
	}
}
