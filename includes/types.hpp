/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   types.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 11:55:56 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/18 11:42:19 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TYPES_HPP
#define TYPES_HPP

# include "./webserv.hpp"

enum RequestMethod { GET, POST, DELETE_ };

struct ClientRequest {
	// Request line
	RequestMethod method;
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
};

#endif
