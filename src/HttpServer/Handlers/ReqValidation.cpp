/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ReqValidation.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 12:56:57 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/25 14:59:07 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/HttpServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/Utils/ServerUtils.hpp"

bool WebServer::matchLocation(ClientRequest &req, Connection *conn) {
	// initialize the correct locConfig // default "/"
	_lggr.debug("Path to match : " + req.path);
	LocConfig *match = findBestMatch(req.path, conn->servConfig->getLocations(), _lggr);
	if (!match) {
		_lggr.error("[Resp] No matched location for : " + req.path);
		prepareResponse(conn, Response::internalServerError(conn));
		return false;
	}
	conn->locConfig = match;
	conn->locConfig->setFullPath("");
	_lggr.debug("[Resp] Matched location : " + conn->locConfig->path);
	return true;
}

bool WebServer::normalizePath(ClientRequest &req, Connection *conn) {

	// normalisation
	_lggr.debug("full_path: " + req.path);
	std::string full_path = buildFullPath(req.path, conn->locConfig);
	std::string root_full_path = buildFullPath("", conn->locConfig);
	char resolved[PATH_MAX];
	realpath(full_path.c_str(), resolved);
	std::string normal_full_path(resolved);
	if (su::back(normal_full_path) != '/')
		normal_full_path += "/";

	_lggr.debug("full_path: " + full_path);
	_lggr.debug("normal_full_path: " + normal_full_path);
	_lggr.debug("root_full_path: " + root_full_path);

	// std::string temp_full_path = normal_full_path + "/";
	if (normal_full_path.compare(0, root_full_path.size(), root_full_path) != 0) {
		_lggr.error("Resolved path is trying to access parent directory: " + normal_full_path);
		prepareResponse(conn, Response::forbidden(conn));
		return false;
	}
	_lggr.debug("[Resp] Normalized full path is safe : " + normal_full_path);
	
	// this should maybe be in the connection info, not in the locConfig
	if (su::back(req.path) != '/' && su::back(normal_full_path) == '/')
		normal_full_path = normal_full_path.substr(0, normal_full_path.length() - 1);
	conn->locConfig->setFullPath(normal_full_path);
	return true;
}

// Max body, Return, Method
bool WebServer::processValidRequestChecks(ClientRequest &req, Connection *conn) {

	// check if RETURN directive in the matched location
	if (conn->locConfig->hasReturn() && conn->locConfig->path == req.path) {
		_lggr.debug("[Resp] The matched location has a return directive.");
		uint16_t code = conn->locConfig->return_code;
		std::string target = conn->locConfig->return_target;
		prepareResponse(conn, respReturnDirective(conn, code, target));
		return false;
	}

	_lggr.debug("[Resp] No return directive (or no exact match)");
	
	// method allowed?
	if (!conn->locConfig->hasMethod(req.method)) {
		_lggr.error("[Resp] Method " + req.method + " is not allowed for location " +
				  conn->locConfig->path);
		prepareResponse(conn, Response::methodNotAllowed(conn, conn->locConfig->getAllowedMethodsString()));
		return false;
	}
	_lggr.debug("[Resp] Method " + req.method + " is allowed (allowed: " 
		         + conn->locConfig->getAllowedMethodsString() + ")");

	if (req.content_length == -1 && req.chunked_encoding == false && req.method != "GET") {
		_lggr.error("No content length, not chunked");
		prepareResponse(conn, Response(411, conn));
		return false;
	}
	
	// Check against location's max body size
	if ((req.content_length != -1) && !conn->locConfig->infiniteBodySize() &&
	    static_cast<size_t>(req.content_length) > conn->locConfig->getMaxBodySize()) {
		_lggr.logWithPrefix(
		    Logger::WARNING, "HTTP",
		    "Request body too large: " + su::humanReadableBytes(req.content_length) +
		        " bytes exceeds limit of " +
		        su::humanReadableBytes(conn->locConfig->getMaxBodySize()));
		prepareResponse(conn, Response::contentTooLarge(conn));
		return false;
	}
	if (req.content_length == -1) {
		_lggr.logWithPrefix(Logger::DEBUG, "HTTP", "No request content length -> ok.");
	} else {
		_lggr.logWithPrefix(
		    Logger::DEBUG, "HTTP",
		    "Request content length is ok: " + su::humanReadableBytes(req.content_length) +
		        " bytes, max is " + su::humanReadableBytes(conn->locConfig->getMaxBodySize()));
	}
	return true;
}
