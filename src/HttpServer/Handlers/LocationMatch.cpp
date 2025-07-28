#include "ConfigParser/config_parser.hpp"
#include "RequestParser/request_parser.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/Utils/StringUtils.hpp"
#include <string>
#include <vector>

static bool isPrefixMatch(const std::string &uri, const std::string &location_path);

LocConfig *findBestMatch(const std::string &uri, std::vector<LocConfig> &locations) {
	for (std::vector<LocConfig>::iterator it = locations.begin(); it != locations.end(); ++it) {
		if (isPrefixMatch(uri, it->path)) {
			return &(*it);
		}
	}
	return NULL;
}

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
