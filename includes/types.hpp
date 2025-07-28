/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   types.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 11:55:56 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/22 14:00:38 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TYPES_HPP
#define TYPES_HPP

# include "./webserv.hpp"
#include <string>

// enum RequestMethod { GET, POST, DELETE_ };

struct ClientRequest {
	// Request line
	std::string method;
	std::string uri;
	std::string path;
	std::string query;
	std::string version;

	// Headers
	std::map<std::string, std::string> headers;
	bool chunked_encoding;

	// Body (optional)
	std::string body;

	// Client FD
	int clfd;

	// CGI request
	bool CGI;

    std::string toString();
};

/* struct Response {
	std::string version;                        // HTTP/1.1
	uint16_t status_code;                       // e.g. 200
	std::string reason_phrase;                  // e.g. OK
	std::map<std::string, std::string> headers; // e.g. Content-Type: text/html
	std::string body;                           // e.g. <h1>Hello world!</h1>

	Response();
	explicit Response(uint16_t code);
	Response(uint16_t code, const std::string &response_body);
	inline void setStatus(uint16_t code);
	inline void setHeader(const std::string &name, const std::string &value);
	inline void setContentType(const std::string &ctype);
	inline void setContentLength(size_t length);
	std::string toString() const;
	// Factory methods for common responses
	static Response ok(const std::string &body = "");
	static Response notFound();
	static Response internalServerError();
	static Response badRequest();
	static Response methodNotAllowed();
  private:
	std::string getReasonPhrase(uint16_t code) const;
	void initFromStatusCode(uint16_t code);
}; */

#endif
