/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Types.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 11:55:56 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:01:30 by jalombar         ###   ########.fr       */
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
	bool file_upload;

	// Body (optional)
	std::string body;

	// Client FD
	int clfd;

	// CGI request
	std::string extension;

	std::string toString();
};

#endif
