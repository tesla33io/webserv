/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ResponseHandler.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:08:41 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 14:17:10 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"

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
