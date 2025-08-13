/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:05:50 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 14:29:46 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include "includes/Webserv.hpp"
#include "src/Logger/Logger.hpp"
#include "src/Utils/StringUtils.hpp"

class Connection;

class Response {
  public:
	std::string version;                        // HTTP/1.1
	uint16_t status_code;                       // e.g. 200
	std::string reason_phrase;                  // e.g. OK
	std::map<std::string, std::string> headers; // e.g. Content-Type: text/html
	std::string body;                           // e.g. <h1>Hello world!</h1>

	Response();
	explicit Response(uint16_t code);
	explicit Response(uint16_t code, const std::string &response_body);
	explicit Response(uint16_t code, Connection *conn); // custom error pages

	inline void setStatus(uint16_t code) {
		status_code = code;
		reason_phrase = getReasonPhrase(code);
	}

	inline void setHeader(const std::string &name, const std::string &value) {
		headers[name] = value;
	}

	inline void setContentType(const std::string &ctype) { headers["Content-Type"] = ctype; }

	inline void setContentLength(size_t length) {
		headers["Content-Length"] = su::to_string(length);
	}

	std::string toString() const;
	std::string toStringHeadersOnly() const;
	std::string toShortString() const;
	void reset();

	// Factory methods for common responses
	static Response continue_();
	static Response ok(const std::string &body = "");
	static Response notFound();
	static Response internalServerError();
	static Response badRequest();
	static Response methodNotAllowed();
	static Response notImplemented();
	static Response forbidden();

	// Factory methods overload when Connexion instance is available
	static Response notFound(Connection *conn);
	static Response internalServerError(Connection *conn);
	static Response badRequest(Connection *conn);
	static Response methodNotAllowed(Connection *conn);
	static Response notImplemented(Connection *conn);
	static Response forbidden(Connection *conn);

  private:
	std::string getReasonPhrase(uint16_t code) const;
	void initFromStatusCode(uint16_t code);
	void initFromCustomErrorPage(uint16_t code, Connection *conn);

	static Logger
	    tmplogg_; // not sure this is the best logic for the logger but i wanted to be able to log
};

#endif /* end of include guard: RESPONSE_HPP */
