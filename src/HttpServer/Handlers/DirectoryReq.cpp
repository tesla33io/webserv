/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   DirectoryReq.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 12:56:57 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 14:16:59 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"

// Serving the index file or listing if possible
Response WebServer::handleDirectoryRequest(Connection *conn, const std::string &fullDirPath) {
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
Response WebServer::handleFileRequest(Connection *conn, const std::string &fullFilePath) {
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
	_lggr.debug("Successfully serving file: " + fullFilePath + " (" +
	            su::to_string(content.length()) + " bytes)");
	return resp;
}

Response WebServer::handleReturnDirective(Connection *conn, uint16_t code, std::string target) {

	_lggr.debug("Handling return directive '" + su::to_string(code) + "' to " + target);
	if (code == 0 || target.empty()) {
		_lggr.error("Problem with the return directive - not properly configured");
		return Response::internalServerError(conn);
	}

	Response resp(code);
	resp.setHeader("Location", target);
	std::ostringstream html;
	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head>\n"
	     << "<title>Redirecting...</title>\n"
	     << "</head>\n"
	     << "<body>\n"
	     << "<h1>Redirecting</h1>\n"
	     << "<p>The document has moved <a href=\"#\">here</a>.</p>\n"
	     << "</body>\n"
	     << "</html>\n";
	resp.body = html.str();
	resp.setContentType("text/html");
	resp.setContentLength(resp.body.length());
	_lggr.debug("Generated redirect response");

	return resp;
}

std::string WebServer::detectContentType(const std::string &path) {

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
	return "application/octet-stream"; // default binary stream
}

std::string WebServer::getExtension(const std::string &path) {
	std::size_t dot_pos = path.find_last_of('.');
	std::size_t qm_pos = path.find_first_of('?');
	if (qm_pos != std::string::npos && dot_pos < qm_pos)
		return path.substr(dot_pos, qm_pos - dot_pos);
	else if (qm_pos == std::string::npos && dot_pos != std::string::npos)
		return path.substr(dot_pos);
	return "";
}

// struct dirent {
//     ino_t          d_ino;       // Inode number
//     char           d_name[256]; // Name of the entry (file or subdirectory)
//     unsigned char  d_type;      // Type of entry (optional, not always available)
// };
Response WebServer::generateDirectoryListing(Connection *conn, const std::string &fullDirPath) {
	_lggr.debug("Generating directory listing for: " + fullDirPath);

	// Open directory
	DIR *dir = opendir(fullDirPath.c_str());
	if (dir == NULL) {
		_lggr.error("Failed to open directory: " + fullDirPath + " - " +
		            std::string(strerror(errno)));
		return Response::notFound(conn);
	}

	// Generate HTML content
	std::ostringstream htmlContent;
	htmlContent
	    << "<!DOCTYPE html>\n"
	    << "<html>\n"
	    << "<head>\n"
	    << "<title>Directory Listing - " << fullDirPath << "</title>\n"
	    << "<style>\n"
	    << "@import "
	       "url('https://fonts.googleapis.com/"
	       "css2?family=Space+Mono:ital,wght@0,400;0,700;1,400;1,700&display=swap');\n"
	    << "body { font-family: 'Space Mono', monospace; background-color: #f8f9fa; margin: 0; "
	       "padding: 40px; }\n"
	    << "h1 { color: #ff5555; font-weight: 700; font-size: 2em; margin-bottom: 30px; }\n"
	    << ".path { color: #6c757d; font-size: 16px; margin-bottom: 20px; }\n"
	    << "table { border-collapse: collapse; width: 100%; background-color: white; "
	       "border-radius: 8px; overflow: hidden; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }\n"
	    << "th { background-color: #343a40; color: white; padding: 15px; text-align: left; "
	       "font-weight: 700; }\n"
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
	    << "<div class=\"path\">" << fullDirPath << "</div>\n"
	    << "<table>\n"
	    << "<tr><th>Name</th><th>Type</th><th>Size</th></tr>\n";

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		std::string filename = entry->d_name;
		std::string fullPath = fullDirPath;
		if (fullPath[fullDirPath.size() - 1] != '/')
			fullPath += "/";
		fullPath += filename;

		if (filename == ".")
			continue;

		// Get file information
		struct stat fileStat;
		if (stat(fullPath.c_str(), &fileStat) == 0) {
			htmlContent << "<tr>"
			            << "<td><a href=\"" << filename; // Name column with link
			if (S_ISDIR(fileStat.st_mode)) {
				htmlContent << "/"; // Add slash for directories
				htmlContent << "\" class=\"dir\">";
			} else {
				htmlContent << "\" class=\"file\">";
			}
			htmlContent << filename << "</a></td>"
			            << "<td>"; // Type column
			if (S_ISDIR(fileStat.st_mode)) {
				htmlContent << "<span class=\"dir\">Directory</span>";
			} else if (S_ISREG(fileStat.st_mode)) {
				htmlContent << "<span class=\"file\">File</span>";
			} else {
				htmlContent << "Other";
			}
			htmlContent << "</td>"
			            << "<td class=\"size\">"; // Size column
			if (S_ISREG(fileStat.st_mode)) {
				htmlContent << su::to_string(fileStat.st_size);
			} else {
				htmlContent << "-";
			}
			htmlContent << "</td>"
			            << "</tr>\n";
		} else { // If stat fails, still show the entry but with unknown info
			htmlContent << "<tr>"
			            << "<td><a href=\"" << filename << "\">" << filename << "</a></td>"
			            << "<td>Unknown</td>"
			            << "<td>-</td>"
			            << "</tr>\n";
		}
	}

	htmlContent << "</table>\n"
	            << "<footer>Generated by WebServer " << __WEBSERV_VERSION__ << "</footer>\n"
	            << "</body>\n</html>";

	closedir(dir);

	// footer

	// Create response
	std::string body = htmlContent.str();
	Response resp(200, body);
	resp.setContentType("text/html");
	resp.setContentLength(body.length());

	_lggr.debug("Generated directory listing (" + su::to_string(body.length()) + " bytes)");
	return resp;
}
