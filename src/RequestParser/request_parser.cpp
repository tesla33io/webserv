/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   request_parser.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/17 15:43:17 by jalombar          #+#    #+#             */
/*   Updated: 2025/06/19 12:18:48 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "request_parser.hpp"

bool RequestParsingUtils::assign_method(std::string &method, ClientRequest &request)
{
	for (std::string::size_type i = 0; i < method.length(); ++i)
        method[i] = std::toupper(static_cast<unsigned char>(method[i]));
	if (method == "GET")
		request.method = GET;
	else if (method == "POST")
		request.method = POST;
	else if (method == "DELETE")
		request.method = DELETE_;
	else
		return (false);
	return (true);
}

std::string RequestParsingUtils::trim(const std::string &s)
{
	std::string result = s;
	while (!result.empty() && (result[0] == ' ' || result[0] == '\t'))
		result.erase(0, 1);
	while (!result.empty() && (result[result.size() - 1] == ' ' || result[result.size() - 1] == '\t'))
		result.erase(result.size() - 1);
	return (result);
}

/* std::string RequestParsingUtils::read_request(int fd)
{
	
} */

bool RequestParsingUtils::parse_req_line(std::istringstream &stream, ClientRequest &request)
{
	std::string line;
	std::string method;

    if (!std::getline(stream, line))
        return (false);

    if (!line.empty() && line[line.length() - 1] == '\r')
        line.erase(line.length() - 1);

    std::istringstream request_line(trim(line));
    if (!(request_line >> method >> request.uri >> request.version))
		return (false);

	if (!assign_method(method, request))
		return (false);
	else
		return (true);
}

bool RequestParsingUtils::parse_headers(std::istringstream &stream, ClientRequest &request)
{
	std::string line;
	while (std::getline(stream, line))
	{
        if (!line.empty() && line[line.length() - 1] == '\r')
            line.erase(line.length() - 1);

        if (line.empty()) // Empty line = end of headers
            break;

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            return (false);
		std::string name = trim(line.substr(0, colon));
		std::string value = trim(line.substr(colon + 1));

        // Trim spaces
        if (!value.empty() && value[0] == ' ')
            value.erase(0, 1);

        request.headers[name] = value;
    }
	return (true);
}

bool RequestParsingUtils::parse_body(std::istringstream &stream, ClientRequest &request)
{
	// Check if Content-Lenght Header exists and size is valid
    std::map<std::string, std::string>::iterator it = request.headers.find("Content-Length");
    if (it == request.headers.end())
        return (false);

    int content_length = std::atoi(it->second.c_str());
    if (content_length <= 0)
        return (false);
	
	// Read exactly content_length bytes
    std::string body(content_length, '\0');
    stream.read(&body[0], content_length);
    std::streamsize actually_read = stream.gcount();

    if (actually_read != content_length)
        return (false); // Incomplete body

    request.body = body;
    return (true);
}

bool RequestParsingUtils::parse_request(const std::string &raw_request, ClientRequest &request)
{
    if (raw_request.empty())
        return (false);
    
    std::istringstream stream(raw_request);

	// Parse request line
	if (!parse_req_line(stream, request))
		return (false);

    // Parse headers
	if (!parse_headers(stream, request))
		return (false);

    // Read body (rest of the stream)
	if (request.method == POST)
	{
		if (!parse_body(stream, request))
			return (false);
	}

    // Remove trailing newline if present
    if (!request.body.empty() && request.body[request.body.length() - 1] == '\n')
        request.body.erase(request.body.length() - 1);

    return (true);
}

/* bool	parse_request(const std::string &raw_request, ClientRequest &request) {
    if (raw_request.empty())
        return (false);
    
    std::istringstream stream(raw_request);
    std::string line;

    // Parse request line
    if (!std::getline(stream, line))
        return (false);

    // Remove \r if present
    if (!line.empty() && line[line.length() - 1] == '\r')
        line.erase(line.length() - 1);

    std::istringstream request_line(trim(line));
    if (!(request_line >> request.method >> request.uri >> request.version))
        return (false);

    // Parse headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.length() - 1] == '\r')
            line.erase(line.length() - 1);

        if (line.empty()) // Empty line = end of headers
            break;

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            return (false);
		std::string name = trim(line.substr(0, colon));
		std::string value = trim(line.substr(colon + 1));

        // Trim spaces
        if (!value.empty() && value[0] == ' ')
            value.erase(0, 1);

        request.headers[name] = value;
    }

    // Read body (rest of the stream)
    std::ostringstream body_stream;
    while (std::getline(stream, line)) {
        body_stream << line << '\n';
    }
    request.body = body_stream.str();

    // Remove trailing newline if present
    if (!request.body.empty() && request.body[request.body.length() - 1] == '\n')
        request.body.erase(request.body.length() - 1);

    return (true);
} */