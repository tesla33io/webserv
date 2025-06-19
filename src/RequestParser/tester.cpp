/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tester.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 11:44:05 by jalombar          #+#    #+#             */
/*   Updated: 2025/06/19 14:13:36 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "request_parser.hpp"

std::vector<std::pair<std::string, std::string> > test_requests;

void init_requests()
{
    test_requests.push_back(std::make_pair("no_content_length",
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "\r\n"
        "username=admin&pass=123"));

    test_requests.push_back(std::make_pair("zero_content_length",
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 0\r\n"
        "\r\n"));

    test_requests.push_back(std::make_pair("body_size_mismatch",
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 25\r\n"
        "\r\n"
        "username=admin&pass=123"));

    test_requests.push_back(std::make_pair("spaces_in_headers",
        "GET /page HTTP/1.1\r\n"
        "Host :   example.com  \r\n"
        "User-Agent:  TestAgent\r\n"
        "\r\n"));

    test_requests.push_back(std::make_pair("invalid_method",
        "FAKE /index HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n"));

    test_requests.push_back(std::make_pair("get_request",
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n"));

    test_requests.push_back(std::make_pair("post_request",
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "name=ChatGPT"));
}

int main(void)
{
	init_requests();
	for (size_t i = 0; i < test_requests.size(); ++i)
	{
        const std::string &name = test_requests[i].first;
        const std::string &raw = test_requests[i].second;

        ClientRequest request;
        bool ok = RequestParsingUtils::parse_request(raw, request);

        std::cout << std::endl << "=== Test: " << name << " ===" << std::endl;
        std::cout << raw << std::endl;
        std::cout << "Result: " << (ok ? "PASS ✅" : "FAIL ❌") << std::endl;
        std::cout << "-----------------------------" << std::endl << std::endl;
    }
	return (0);
}