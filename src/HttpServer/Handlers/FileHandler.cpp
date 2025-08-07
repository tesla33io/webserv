/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileHandler.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:07:32 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:08:58 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/HttpServer.hpp"

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

FileType checkFileType(std::string path) {
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
