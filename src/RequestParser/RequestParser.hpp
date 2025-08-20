/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestParser.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: htharrau <htharrau@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/17 15:32:36 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/19 16:48:06 by htharrau         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include "includes/Types.hpp"
#include "includes/Webserv.hpp"
#include "src/Logger/Logger.hpp"
#include "src/Utils/GeneralUtils.hpp"
#include "src/Utils/StringUtils.hpp"

const size_t MAX_URI_LENGTH = 2048;
const size_t MAX_HEADER_NAME_LENGTH = 1024;
const size_t MAX_HEADER_VALUE_LENGTH = 8000;

namespace RequestParsingUtils {
uint16_t checkNTrimLine(std::string &line, Logger &logger);
const char *findHeader(ClientRequest &request, const std::string &header, Logger &logger);
std::string trimSide(const std::string &s, int type);
uint16_t checkReqLine(ClientRequest &request, Logger &logger);
uint16_t parseReqLine(std::istringstream &stream, ClientRequest &request, Logger &logger);
uint16_t checkHeader(std::string &name, std::string &value, ClientRequest &request, Logger &logger);
uint16_t parseHeaders(std::istringstream &stream, ClientRequest &request, Logger &logger);
uint16_t parseBody(std::istringstream &stream, ClientRequest &request, Logger &logger);
uint16_t parseTrailingHeaders(std::istringstream &stream, ClientRequest &request, Logger &logger);
uint16_t parseRequest(const std::string &raw_request, ClientRequest &request, Logger &logger);
uint16_t parseRequestHeaders(const std::string &raw_request, ClientRequest &request, Logger &logger);
} // namespace RequestParsingUtils

#endif
