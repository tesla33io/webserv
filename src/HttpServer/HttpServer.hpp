/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpServer.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/15 11:02:08 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/18 15:37:01 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP

#include "includes/Webserv.hpp"
#include "src/CGI/CGI.hpp"
#include "src/ConfigParser/Struct.hpp"
#include "src/Logger/Logger.hpp"
#include "src/RequestParser/RequestParser.hpp"
#include "src/Utils/ArgumentParser.hpp"
#include "src/Utils/GeneralUtils.hpp"
#include "src/Utils/ServerUtils.hpp"
#include "src/Utils/StringUtils.hpp"

#define KEEP_ALIVE_TO 5 // seconds
#define MAX_KEEP_ALIVE_REQS 100
#define MAX_EVENTS 4096

#ifndef uint16_t
#define uint16_t unsigned short
#endif // uint16_t

#define __WEBSERV_VERSION__ "whateverX 0.whatever.7 -- made with <3 at 42 Berlin"



#endif
