/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ReqValidation.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/08 12:56:57 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/19 21:32:44 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"
#include "src/Utils/ServerUtils.hpp"

bool WebServer::matchLocation(ClientRequest &req, Connection *conn) {
	// initialize the correct locConfig // default "/"
	LocConfig *match = findBestMatch(req.uri, conn->servConfig->getLocations());
	if (!match) {
		_lggr.error("[Resp] No matched location for : " + req.uri);
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
	std::string full_path = buildFullPath(req.uri, conn->locConfig);
	std::string root_full_path = buildFullPath("", conn->locConfig);
	char resolved[PATH_MAX];
	realpath(full_path.c_str(), resolved);
	std::string normal_full_path(resolved);
	if (su::back(full_path) == '/')
		normal_full_path += "/";
	
	// std::string temp_full_path = normal_full_path + "/";
	if (normal_full_path.compare(0, root_full_path.size(), root_full_path) != 0) {
		_lggr.error("Resolved path is trying to access parent directory: " + normal_full_path);
		prepareResponse(conn, Response::forbidden(conn));
		return false;
	}
	_lggr.debug("[Resp] The normalized full path is safe : " + normal_full_path);
	
	// this should maybe be in the connection info, not in the locConfig
	conn->locConfig->setFullPath(normal_full_path);
	return true;
}

// Max body, Return, Method
bool WebServer::processValidRequestChecks(ClientRequest &req, Connection *conn) {
	
	// check if RETURN directive in the matched location
	if (conn->locConfig->hasReturn() && conn->locConfig->is_exact_()) {
		_lggr.debug("[Resp] The matched location has a return directive.");
		uint16_t code = conn->locConfig->return_code;
		std::string target = conn->locConfig->return_target;
		prepareResponse(conn, respReturnDirective(conn, code, target));
		return false;
	}
	_lggr.debug("[Resp] The matched location does not have return directive or the match is not exact.");
	
	// method allowed?
	if (!conn->locConfig->hasMethod(req.method)) {
		_lggr.warn("[Resp] Method " + req.method + " is not allowed for location " +
				  conn->locConfig->path);
		prepareResponse(conn, Response::methodNotAllowed(conn, conn->locConfig->getAllowedMethodsString()));
		return false;
	}
	_lggr.debug("[Resp] Method " + req.method + " is allowed " + conn->locConfig->getAllowedMethodsString());
	
	// Check against location's max body size
	if ((req.content_length != -1) && !conn->locConfig->infiniteBodySize() && 
		static_cast<size_t>(req.content_length) > conn->locConfig->getMaxBodySize()) {
		_lggr.logWithPrefix(Logger::WARNING, "HTTP", 
						 "Request body too large: " + su::humanReadableBytes(req.content_length) + 
							 " bytes exceeds limit of " + su::humanReadableBytes(conn->locConfig->getMaxBodySize()));
		prepareResponse(conn, Response::contentTooLarge(conn));
		return false;
	}
	if (req.content_length == -1) {
		_lggr.logWithPrefix(Logger::DEBUG, "HTTP",  "No request content length -> ok.");
	} else {
		_lggr.logWithPrefix(Logger::DEBUG, "HTTP",  "Request content length is ok: " 
		                       + su::humanReadableBytes(req.content_length) + 
		                            " bytes, max is " + su::humanReadableBytes(conn->locConfig->getMaxBodySize()));
	}
	return true;
}

