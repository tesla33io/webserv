/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerUtils.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 13:19:18 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 14:19:03 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"

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

FileType WebServer::checkFileType(std::string path) {
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
	std::string prefix = (_root_prefix_path[_root_prefix_path.length() - 1] == '/')
	                         ? _root_prefix_path.substr(0, _root_prefix_path.length() - 1)
	                         : _root_prefix_path;
	std::string root = (location->root[location->root.length() - 1] == '/')
	                       ? location->root.substr(0, location->root.length() - 1)
	                       : location->root;
	std::string slashedUri = (uri.empty() || uri[0] != '/') ? "/" + uri : uri;

	std::string full_path = prefix + root + slashedUri;
	_lggr.debug("Path building:");
	_lggr.debug("  - prefix: '" + _root_prefix_path + "'");
	_lggr.debug("  - root: '" + location->root + "'");
	_lggr.debug("  - uri: '" + uri + "'");
	_lggr.debug("  - result: '" + full_path + "'");

	return full_path;
}
