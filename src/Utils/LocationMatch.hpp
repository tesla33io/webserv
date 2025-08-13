/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationMatch.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:01:13 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 13:14:14 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LOCATIONMATCH_HPP
#define LOCATIONMATCH_HPP

#include "includes/Webserv.hpp"
#include "src/ConfigParser/ConfigParser.hpp"

static bool isPrefixMatch(const std::string &uri, const std::string &location_path) {
	if (location_path.empty() || location_path == "/") {
		return true;
	}
	if (uri.length() < location_path.length()) {
		return false;
	}
	// Check for prefix
	if (uri.substr(0, location_path.length()) != location_path) {
		return false;
	}
	if (uri.length() == location_path.length()) {
		return true; // Exact match
	}
	// Next character should be '/' or end of string
	char next_char = uri[location_path.length()];
	return next_char == '/' || location_path[location_path.length() - 1] == '/';
}

/// Finds the location configuration that best matches a URI.
/// \note The logic is very basic for now and needs to be improved in the future
/// \param uri The URI to match against location patterns.
/// \param locations Vector of location configurations to search.
/// \returns Pointer to the best matching LocConfig, or nullptr if no match.
inline LocConfig *findBestMatch(const std::string &uri, std::vector<LocConfig> &locations) {
	for (std::vector<LocConfig>::iterator it = locations.begin(); it != locations.end(); ++it) {
		if (isPrefixMatch(uri, it->getPath())) {
			return &(*it);
		}
	}
	return NULL;
}

#endif
