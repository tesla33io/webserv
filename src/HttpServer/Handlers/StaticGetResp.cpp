/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticGetResp.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/19 18:29:33 by htharrau          #+#    #+#             */
/*   Updated: 2025/08/21 10:35:04 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/HttpServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/Utils/ServerUtils.hpp"

bool WebServer::handleFileSystemErrors(FileType file_type, const std::string &full_path,
                                       Connection *conn) {
	if (file_type == NOT_FOUND_404) {
		_lggr.debug("[Resp] Could not open : " + full_path);
		prepareResponse(conn, Response::notFound(conn));
		return false;
	}
	if (file_type == PERMISSION_DENIED_403) {
		_lggr.debug("[Resp] Permission denied : " + full_path);
		prepareResponse(conn, Response::forbidden(conn));
		return false;
	}
	if (file_type == FILE_SYSTEM_ERROR_500) {
		_lggr.debug("[Resp] Other file access problem : " + full_path);
		prepareResponse(conn, Response::internalServerError(conn));
		return false;
	}
	return true;
}

void WebServer::handleDirectoryRequest(ClientRequest &req, Connection *conn, bool end_slash) {

	const std::string full_path = conn->locConfig->getFullPath();

	_lggr.debug("Directory request: " + full_path);

	if (!end_slash) {
		_lggr.debug("Directory request without trailing slash, redirecting to : " + req.path + "/");
		std::string redirectPath = req.path + "/";
		prepareResponse(conn, respReturnDirective(conn, 301, redirectPath));
		return;
	} else {
		prepareResponse(conn, respDirectoryRequest(conn, full_path));
		return;
	}
}

void WebServer::handleFileRequest(ClientRequest &req, Connection *conn, bool end_slash) {

	const std::string full_path = conn->locConfig->getFullPath();
	_lggr.debug("File request: " + full_path);

	// Trailing '/'? Redirect
	if (end_slash) { //&& !conn->locConfig->is_exact_()
		_lggr.debug("File request with trailing slash, redirecting: " + req.path);
		std::string redirectPath = req.path.substr(0, req.path.length() - 1);
		prepareResponse(conn, respReturnDirective(conn, 301, redirectPath));
		return;
	}
	
	// HANDLE CGI
	std::string extension = getExtension(full_path);
	if (conn->locConfig->acceptExtension(extension)) {
		std::string interpreter = conn->locConfig->getInterpreter(extension);
		_lggr.debug("CGI request, interpreter location : " + interpreter);
		req.extension = extension;
		uint16_t exit_code = handleCGIRequest(req, conn);
		if (exit_code) {
			_lggr.error("Handling the CGI request failed.");
			prepareResponse(conn, Response(exit_code, conn));
		}
		return;
	}

	// HANDLE STATIC GET RESPONSE
	if (req.method == "GET") {
		_lggr.debug("Static file GET request");
		prepareResponse(conn, respFileRequest(conn, full_path));
		return;
	} else {
		_lggr.debug("Non-GET request for static file - not implemented");
		prepareResponse(
		    conn, Response::methodNotAllowed(conn, conn->locConfig->getAllowedMethodsString()));
		return;
	}
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
    struct dirent *entry;
    htmlContent << "<!DOCTYPE html>\n"
                << "<html lang=\"en\">\n"
                << "<head>\n"
                << "<meta charset=\"UTF-8\">\n"
                << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                << "<title>Directory Listing - " << fullDirPath << "</title>\n"
                << "<link rel=\"stylesheet\" href=\"/styles.css\">\n"
                << "</head>\n<body>\n"
                << "<div class=\"container\">\n"
                << "<h1 class=\"title\">Directory Listing</h1>\n"
                << "<p class=\"subtitle\">" << fullDirPath << "</p>\n"
                << "<table>\n<tr><th>Name</th><th>Type</th><th>Size</th></tr>\n";

    while ((entry = readdir(dir)) != NULL) {
        std::string filename = entry->d_name;
        if (filename == ".")
            continue;

        std::string fullPath = fullDirPath;
        if (su::back(fullPath) != '/')
            fullPath += "/";
        fullPath += filename;

        struct stat fileStat;
        htmlContent << "<tr><td><a href=\"" << filename;
        if (stat(fullPath.c_str(), &fileStat) == 0) {
            if (S_ISDIR(fileStat.st_mode)) {
                htmlContent << "/\" class=\"dir\">";
            } else {
                htmlContent << "\" class=\"file\">";
            }
            htmlContent << filename << "</a></td><td>";
            if (S_ISDIR(fileStat.st_mode))
                htmlContent << "<span class=\"dir\">Directory</span>";
            else if (S_ISREG(fileStat.st_mode))
                htmlContent << "<span class=\"file\">File</span>";
            else
                htmlContent << "Other";

            htmlContent << "</td><td class=\"size\">";
            if (S_ISREG(fileStat.st_mode))
                htmlContent << su::to_string(fileStat.st_size);
            else
                htmlContent << "-";
            htmlContent << "</td></tr>\n";
        } else {
            htmlContent << "\">" << filename << "</a></td><td>Unknown</td><td>-</td></tr>\n";
        }
    }

    htmlContent << "</table>\n"
                << "<footer>Generated by WebServer " << __WEBSERV_VERSION__ << "</footer>\n"
                << "</div>\n"
                << "<div class=\"floating-elements\">\n"
                << "<div class=\"floating-element\"></div>\n"
                << "<div class=\"floating-element\"></div>\n"
                << "<div class=\"floating-element\"></div>\n"
                << "</div>\n"
                << "</body>\n</html>";

    closedir(dir);

    // Create response
    std::string body = htmlContent.str();
    Response resp(200, body);
    resp.setContentType("text/html");
    resp.setContentLength(body.length());

    _lggr.debug("Generated directory listing (" + su::to_string(body.length()) + " bytes)");
    return resp;
}

/* Response WebServer::generateDirectoryListing(Connection *conn, const std::string &fullDirPath) {
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
		if (su::back(fullPath) != '/')
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
} */
