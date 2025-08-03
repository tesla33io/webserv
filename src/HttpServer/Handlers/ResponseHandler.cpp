#include "ConfigParser/config_parser.hpp"
#include "RequestParser/request_parser.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/Utils/StringUtils.hpp"
#include <cstdio>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <vector>


ssize_t WebServer::prepareResponse(Connection *conn, const Response &resp) {
	// TODO: some checks if the arguments are fine to work with
	// TODO: make sure that Response has all required headers set up correctly (e.g. Content-Type,
	// Content-Length, etc).
	if (conn->response_ready) {
		_lggr.error(
			"Trying to prepare a response for a connection that is ready to sent another one");
		_lggr.error("Current response: " + conn->response.toShortString());
		_lggr.error("Trying to prepare response: " + resp.toShortString());
		return -1;
	}
	_lggr.debug("Saving a response [" + su::to_string(resp.status_code) + "] for fd " +
				su::to_string(conn->fd));
	conn->response = resp;
	conn->response_ready = true;
	return conn->response.toString().size();
	// return send(clfd, raw_response.c_str(), raw_response.length(), 0);
}


bool WebServer::sendResponse(Connection *conn) {
	if (!conn->response_ready) {
		_lggr.error("Response is not ready to be sent back to the client");
		_lggr.debug("Error for clinet " + conn->toString());
		return false;
	}
	_lggr.debug("Sending response [" + conn->response.toShortString() +
				"] back to fd: " + su::to_string(conn->fd));
	std::string raw_response = conn->response.toString();
	epollManage(EPOLL_CTL_MOD, conn->fd, EPOLLIN);
	conn->response.reset();
	conn->response_ready = false;
	return send(conn->fd, raw_response.c_str(), raw_response.size(), MSG_NOSIGNAL) != -1;
}


Response WebServer::handleGetRequest(ClientRequest &req, Connection *conn) {
	_lggr.debug("Requested path: " + req.uri);
	
	// WE PASS CONNECTION AS ARGUMENT
	// Connection* conn = getConnection(req.clfd);
	// if (!conn || !conn->servConfig) {
	// 	_lggr.error("[Resp] Connection instance not found for client fd : " + su::to_string(req.clfd));
	// 	return Response::internalServerError(conn);
	// }
	
	// Build file path
	std::string fullPath = buildFullPath(req.uri, conn->locConfig);

	if (checkFileType(fullPath) == NOT_FOUND_404){
		_lggr.debug("Could not open : " + fullPath);
		return Response::notFound(conn);
	} else if (checkFileType(fullPath) == PERMISSION_DENIED_403){
		_lggr.debug("Permission denied : " + fullPath);
		return Response::forbidden(conn);
	} else if (checkFileType(fullPath) == FILE_SYSTEM_ERROR_500){
		_lggr.debug("Other file access problem : " + fullPath);
		return Response::internalServerError(conn);
	} 
	
	else if (checkFileType(fullPath) == ISDIR) {
		_lggr.debug("Directory request : " + fullPath);
		return handleDirectoryRequest(conn, fullPath);
	}
	else if (checkFileType(fullPath) == ISREG) {
		_lggr.error("File request: " + fullPath);
		return handleFileRequest(conn, fullPath);
	}
	return Response::internalServerError(conn);
}

// Serving the index file or listing if possible
Response WebServer::handleDirectoryRequest(Connection* conn, const std::string& fullDirPath) {
	_lggr.debug("Handling directory request: " + fullDirPath);
		
	// Try to serve index file 
	if (!conn->locConfig->index.empty()) {
		std::string fullIndexPath = fullDirPath + conn->locConfig->index;
		_lggr.debug("Trying index file: " + fullIndexPath);
		if (checkFileType(fullIndexPath.c_str()) == ISREG) {
			_lggr.debug("Found index file, serving: " + fullIndexPath);
			return handleFileRequest(conn, fullIndexPath);
		}
	}
	
	// Handle autoindex 
	if (conn->locConfig->autoindex) {
		_lggr.debug("Autoindex on, generating directory listing");
		return generateDirectoryListing(conn, fullDirPath);
	}
	
	// No index file and no autoindex
	_lggr.debug("No index file, autoindex disabled");
	return Response::notFound(conn);
}


// serving the file if found
Response WebServer::handleFileRequest(Connection* conn, const std::string& fullFilePath) {
	_lggr.debug("Handling file request: " + fullFilePath);
	// Read file content
	std::string content = getFileContent(fullFilePath);
	if (content.empty()) {
		_lggr.error("Failed to read file: " + fullFilePath);
		return Response::internalServerError(conn);
	}
	// Create response
	Response resp(200, content);
	resp.setContentType(detectContentType(fullFilePath)); 
	resp.setContentLength(content.length()); 
	_lggr.debug("Successfully serving file: " + fullFilePath + 
				" (" + su::to_string(content.length()) + " bytes)");
	return resp;
}


std::string detectContentType(const std::string &path) {

	std::map<std::string, std::string> cTypes ;
	cTypes[".css"]  = "text/css";
	cTypes[".js"]   = "application/javascript";
	cTypes[".html"] = "text/html";
	cTypes[".htm"]  = "text/html";
	cTypes[".css"]  = "text/css";
	cTypes[".js"]   = "application/javascript";
	cTypes[".json"] = "application/json";
	cTypes[".png"]  = "image/png";
	cTypes[".jpg"]  = "image/jpeg";
	cTypes[".jpeg"] = "image/jpeg";
	cTypes[".gif"]  = "image/gif";
	cTypes[".svg"]  = "image/svg+xml";
	cTypes[".ico"]  = "image/x-icon";
	cTypes[".txt"]  = "text/plain";
	cTypes[".pdf"]  = "application/pdf";
	cTypes[".zip"]  = "application/zip";

	std::string ext = getExtension(path);
	std::map<std::string, std::string>::const_iterator it = cTypes.find(ext); 
	if (it != cTypes.end()) 
		return it->second;
	return "application/octet-stream"; // default binary stream
}

std::string getExtension(const std::string& path) {
	std::size_t dot_pos = path.find('.');
	if (dot_pos != std::string::npos)
		return path.substr(dot_pos);
	return "";
}


Response WebServer::handleReturnDirective(ClientRequest &req, Connection* conn) {
	(void)req;
	_lggr.debug("handleReturnDirective " );
	return Response::notImplemented(conn);

}

// struct dirent {
//     ino_t          d_ino;       // Inode number
//     char           d_name[256]; // Name of the entry (file or subdirectory)
//     unsigned char  d_type;      // Type of entry (optional, not always available)
// };


// Helper function to generate file entry with stat info
std::string generateFileEntry(const std::string& filename, const struct stat& fileStat) {
	std::string entry = "<tr>";
	
	// Name column with link
	entry += "<td><a href=\"" + filename;
	if (S_ISDIR(fileStat.st_mode)) {
		entry += "/";  // Add slash for directories
		entry += "\" class=\"dir\">";
	} else {
		entry += "\" class=\"file\">";
	}
	entry += filename + "</a></td>";
	
	// Type column
	entry += "<td>";
	if (S_ISDIR(fileStat.st_mode)) {
		entry += "<span class=\"dir\"> Directory</span>";
	} else if (S_ISREG(fileStat.st_mode)) {
		entry += "<span class=\"file\"> File</span>";
	} else {
		entry += " Other";
	}
	entry += "</td>";
	
	// Size column
	entry += "<td class=\"size\">";
	if (S_ISREG(fileStat.st_mode)) 
		entry += su::to_string(fileStat.st_size);
	else 
		entry += "-";
	entry += "</td>";
	entry += "</tr>\n";
	return entry;
}


Response WebServer::generateDirectoryListing(Connection* conn, const std::string &fullDirPath) {
	_lggr.debug("Generating directory listing for: " + fullDirPath);
	
	// Open directory
	DIR* dir = opendir(fullDirPath.c_str());
	if (dir == NULL) {
		_lggr.error("Failed to open directory: " + fullDirPath + " - " + std::string(strerror(errno)));
		return Response::notFound(conn);
	}
	
	// Generate HTML content
	std::ostringstream htmlContent;
	htmlContent << "<!DOCTYPE html>\n"
				<< "<html>\n"
				<< "<head>\n"
				<< "<title>Directory Listing - " + fullDirPath + "</title>\n"
				<< "<style>\n"
				<< "@import url('https://fonts.googleapis.com/css2?family=Space+Mono:ital,wght@0,400;0,700;1,400;1,700&display=swap');\n"
				<< "body { font-family: 'Space Mono', monospace; background-color: #f8f9fa; margin: 0; padding: 40px; }\n"
				<< "h1 { color: #ff5555; font-weight: 700; font-size: 2em; margin-bottom: 30px; }\n"
				<< ".path { color: #6c757d; font-size: 16px; margin-bottom: 20px; }\n"
				<< "table { border-collapse: collapse; width: 100%; background-color: white; border-radius: 8px; overflow: hidden; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }\n"
				<< "th { background-color: #343a40; color: white; padding: 15px; text-align: left; font-weight: 700; }\n"
				<< "td { padding: 12px 15px; border-bottom: 1px solid #dee2e6; }\n"
				<< "tr:last-child td { border-bottom: none; }\n"
				<< "tr:hover { background-color: #f8f9fa; }\n"
				<< "a { text-decoration: none; color: #007bff; font-weight: 400; }\n"
				<< "a:hover { text-decoration: underline; color: #0056b3; }\n"
				<< ".dir { color: #ff6b35; font-weight: 700; }\n"
				<< ".file { color: #28a745; }\n"
				<< ".size { color: #6c757d; font-size: 0.9em; }\n"
				<< "footer { color: #6c757d; text-align: center; margin-top: 40px; font-size: 0.9em; }\n"
				<< "</style>\n</head>\n<body>\n"
				<< "<h1>Directory Listing</h1>\n"
				<< "<div class=\"path\">" + fullDirPath + "</div>\n"
				<< "<table>\n"
				<< "<tr><th>Name</th><th>Type</th><th>Size</th></tr>\n";
	
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		std::string filename = entry->d_name;
		std::string fullPath = fullDirPath;
		if (fullPath[fullDirPath.size() - 1] != '/') fullPath += "/";
		fullPath += filename;

		if (filename == ".")
			continue;
		
		// Get file information
		struct stat fileStat;
		if (stat(fullPath.c_str(), &fileStat) == 0) 
			htmlContent << generateFileEntry(filename, fileStat);
		else // If stat fails, still show the entry but with unknown info
			htmlContent << "<tr>"
						<< "<td><a href=\"" + filename + "\">" + filename + "</a></td>"
						<< "<td>Unknown</td>"
						<< "<td>-</td>"
						<< "</tr>\n";
	
	}
	
	closedir(dir);

	// footer
	htmlContent << "</table>\n<footer>" + std::string(__WEBSERV_VERSION__) + "</footer>\n</body>\n</html>\n";
	
	// Create response
	std::string body = htmlContent.str();
	Response resp(200, body);
	resp.setContentType("text/html");
	resp.setContentLength(body.length());
	
	_lggr.debug("Generated directory listing (" + su::to_string(body.length()) + " bytes)");
	return resp;
}
