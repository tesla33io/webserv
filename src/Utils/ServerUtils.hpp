/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerUtils.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/17 19:03:21 by htharrau          #+#    #+#             */
/*   Updated: 2025/08/24 00:10:05 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

// ServerUtils.hpp
#ifndef SERVERUTILS_HPP
#define SERVERUTILS_HPP

#include <string>
#include "src/HttpServer/Structs/WebServer.hpp"
#include "src/HttpServer/Structs/Connection.hpp"
#include "src/HttpServer/Structs/Response.hpp"
#include "src/HttpServer/HttpServer.hpp"

std::string getExtensionForMime(const std::string &path);
std::string detectContentTypeLocal(const std::string &path);
std::string getExtension(const std::string &path);
std::string detectContentType(const std::string &path);
bool isPrefixMatch(const std::string &uri, LocConfig &loc, Logger &log);
LocConfig *findBestMatch(const std::string &uri, std::vector<LocConfig> &locations, Logger &log);
std::string fileTypeToString(FileType type);

#endif
