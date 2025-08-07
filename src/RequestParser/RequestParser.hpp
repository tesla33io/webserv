/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestParser.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/17 15:32:36 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:18:32 by jalombar         ###   ########.fr       */
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
bool checkNTrimLine(std::string &line);
const char *findHeader(ClientRequest &request, const std::string &header);
std::string trimSide(const std::string &s, int type);
bool checkReqLine(ClientRequest &request);
bool parseReqLine(std::istringstream &stream, ClientRequest &request);
bool checkHeader(std::string &name, std::string &value, ClientRequest &request);
bool parseHeaders(std::istringstream &stream, ClientRequest &request);
bool parseBody(std::istringstream &stream, ClientRequest &request);
bool parseTrailingHeaders(std::istringstream &stream, ClientRequest &request);
bool parseRequest(const std::string &raw_request, ClientRequest &request);
} // namespace RequestParsingUtils

#endif
