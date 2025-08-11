/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationMatch.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:07:52 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/11 12:55:22 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/HttpServer.hpp"

static bool isPrefixMatch(const std::string &uri, const LocConfig &loc);

// locations are sorted from longest path to shortest ("/")
// every server has at least one
LocConfig *findBestMatch(const std::string &uri, std::vector<LocConfig> &locations) {
	for (std::vector<LocConfig>::iterator it = locations.begin(); it != locations.end(); ++it) {
		if (isPrefixMatch(uri, *it)) {
			return &(*it);
		}
	}
	return NULL;
}

static bool isPrefixMatch(const std::string &uri, const LocConfig &loc) {
	const std::string &location_path = loc.getPath();
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
	 if (loc.is_exact_()) {
		return false;
	}
	// Next character should be '/' or end of string
	char next_char = uri[location_path.length()];
	return next_char == '/' || location_path[location_path.length() - 1] == '/';
}

std::string WebServer::buildFullPath(const std::string &uri, LocConfig *location) {
	std::string prefix = (_root_prefix_path[_root_prefix_path.length() - 1] == '/')
	                         ? _root_prefix_path.substr(0, _root_prefix_path.length() - 1)
	                         : _root_prefix_path;
	std::string root = (location->root[location->root.length() - 1] == '/')
	                       ? location->root.substr(0, location->root.length() - 1)
	                       : location->root;
	std::string slashed_uri = (uri.empty() || uri[0] != '/') ? "/" + uri : uri;

	std::string full_path = prefix + root + slashed_uri;
	_lggr.debug("Path building:");
	_lggr.debug("  - prefix: '" + _root_prefix_path + "'");
	_lggr.debug("  - root: '" + location->root + "'");
	_lggr.debug("  - uri: '" + uri + "'");
	_lggr.debug("  - result: '" + full_path + "'");

	return full_path;
}
