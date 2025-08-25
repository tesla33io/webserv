/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Types.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 11:55:56 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/25 14:23:27 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TYPES_HPP
#define TYPES_HPP

#include "Webserv.hpp"

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
	ssize_t content_length;
	bool file_upload;

	// Body (optional)
	std::string body;

	// Client FD
	int clfd;

	// CGI request
	std::string extension;

	public:
	
	std::string toString() const;
	std::string printRequest() const;
	ClientRequest() : 
			chunked_encoding(false),
			content_length(-1),
			file_upload(false) {};

};

#endif
