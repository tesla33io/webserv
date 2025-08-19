/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerUtils.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:19:18 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/19 17:54:00 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/Utils/ServerUtils.hpp"

time_t WebServer::getCurrentTime() const { return time(NULL); }

std::string WebServer::getFileContent(std::string path) {
	std::string content;
	std::ifstream file;

	file.open(path.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		_lggr.error("Couldn't open the file (" + path + ")");
		return std::string();
	} else {
		std::stringstream buffer;
		buffer << file.rdbuf();
		file.close();
		content = buffer.str();
		_lggr.logWithPrefix(Logger::DEBUG, "File Handling",
		                    "Read " + su::to_string(content.size()) + " bytes from " + path);
	}
	return content;
}

FileType WebServer::checkFileType(const std::string &path) {
	struct stat pathStat;
	if (stat(path.c_str(), &pathStat) != 0) {
		if (errno == ENOTDIR || errno == ENOENT) {
			return NOT_FOUND_404;
		} else if (errno == EACCES) {
			return PERMISSION_DENIED_403;
		} else {
			return FILE_SYSTEM_ERROR_500;
		}
	}
	if (S_ISDIR(pathStat.st_mode))
		return ISDIR;
	else if (S_ISREG(pathStat.st_mode))
		return ISREG;
	return FILE_SYSTEM_ERROR_500;
}

std::string WebServer::buildFullPath(const std::string &uri, LocConfig *location) {

	std::string root = (su::back(location->root) == '/')
	                       ? location->root.substr(0, location->root.length() - 1)
	                       : location->root;
	std::string front_slashed_uri = (uri.empty() || uri[0] != '/') ? "/" + uri : uri;

	std::string full_path = root + front_slashed_uri;

	return full_path;
}


std::string getExtension(const std::string &path) {
	std::size_t dot_pos = path.find_last_of('.');
	std::size_t qm_pos = path.find_first_of('?');
	if (qm_pos != std::string::npos && dot_pos < qm_pos)
		return path.substr(dot_pos, qm_pos - dot_pos);
	else if (qm_pos == std::string::npos && dot_pos != std::string::npos)
		return path.substr(dot_pos);
	return "";
}

std::string detectContentType(const std::string &path) {

	std::map<std::string, std::string> cTypes;
	cTypes[".css"] = "text/css";
	cTypes[".js"] = "application/javascript";
	cTypes[".html"] = "text/html";
	cTypes[".htm"] = "text/html";
	cTypes[".json"] = "application/json";
	cTypes[".png"] = "image/png";
	cTypes[".jpg"] = "image/jpeg";
	cTypes[".jpeg"] = "image/jpeg";
	cTypes[".gif"] = "image/gif";
	cTypes[".svg"] = "image/svg+xml";
	cTypes[".ico"] = "image/x-icon";
	cTypes[".txt"] = "text/plain";
	cTypes[".pdf"] = "application/pdf";
	cTypes[".zip"] = "application/zip";

	std::string ext = getExtension(path);
	std::map<std::string, std::string>::const_iterator it = cTypes.find(ext);
	if (it != cTypes.end())
		return it->second;
	return "application/octet-stream";
}

LocConfig *findBestMatch(const std::string &uri, std::vector<LocConfig> &locations) {
	for (std::vector<LocConfig>::iterator it = locations.begin(); it != locations.end(); ++it) {
		if (isPrefixMatch(uri, *it)) {
			return &(*it);
		}
	}
	return NULL;
}

bool isPrefixMatch(const std::string &uri, LocConfig &loc) {
	Logger log;
	std::string location_path = loc.getPath();

	// reached only at last since the locconfig are sorted -> no better match 
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
		loc.setExact(true);
		log.debug("EXACT PATH MATCH - uri : " + uri + " loc : " + location_path);
		return true; // Exact match
	}
	if (loc.is_exact_()) {
		return false;
	}
	// Next character should be '/' or end of string
	char next_char = uri[location_path.length()];
	return next_char == '/' || location_path[location_path.length() - 1] == '/';
}
