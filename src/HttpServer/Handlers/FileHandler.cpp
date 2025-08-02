#include "src/HttpServer/HttpServer.hpp"
#include "src/Logger/Logger.hpp"
#include "src/Utils/StringUtils.hpp"
#include <fstream>
#include <sys/stat.h>

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

FileResult checkFileType(std::string path) {
	struct stat pathStat;
		if (stat(path.c_str(), &pathStat) != 0) {
		std::cout << "err number from check file" << errno << std::endl;
			if (errno == ENOTDIR || errno == ENOENT) {
				return NOT_FOUND_404;
			} else if (errno == EACCES) {
				return PERMISSION_DENIED_403;
			} else {
				return FILE_SYSTEM_ERROR_500;
			}
		}
		if S_ISDIR(pathStat.st_mode)
			return ISDIR;
		else if S_ISREG(pathStat.st_mode)
			return ISREG;
		return FILE_SYSTEM_ERROR_500;
}


// bool isDirectory(const char *path) {
// 	struct stat pathStat;
// 	if (stat(path, &pathStat) != 0) {
// 		// TODO: maybe handle this error here
// 		return false;
// 	}
// 	return S_ISDIR(pathStat.st_mode);
// }

// bool isRegularFile(const char *path) {
// 	struct stat pathStat;
// 	if (stat(path, &pathStat) != 0) {
// 		// TODO: maybe handle this error here
// 		return false;
// 	}
// 	return S_ISREG(pathStat.st_mode);
// }


