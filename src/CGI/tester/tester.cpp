/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tester.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/01 08:40:30 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/18 17:33:45 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../cgi.hpp"

std::vector<ClientRequest> test_requests;

void init_requests() {
	ClientRequest test1;
	test1.method = GET;
	test1.path = "cgi-bin/ciao.php";
	test1.query = "name=Claudio";
	test1.uri = test1.path + "?" + test1.query;
	test1.version = "HTTP/1.1";
	test1.headers.clear();
	test1.headers.insert(std::make_pair("host", "localhost"));
	test1.headers.insert(std::make_pair("user-agent", "TestAgent/1.0"));
	test1.headers.insert(std::make_pair("accept", "*/*"));
	test1.chunked_encoding = false;
	test1.body = "";
	test1.clfd = -1;
	test_requests.push_back(test1);

	ClientRequest test2;
	test2.method = POST;
	test2.path = "cgi-bin/ciao.php";
	test2.query = "";
	test2.uri = test2.path;
	test2.version = "HTTP/1.1";
	test2.headers.clear();
	test2.headers.insert(std::make_pair("host", "localhost"));
	test2.headers.insert(std::make_pair("content-type", "application/x-www-form-urlencoded"));
	test2.headers.insert(std::make_pair("content-length", "12"));
	test2.headers.insert(std::make_pair("user-agent", "TestAgent/1.0"));
	test2.headers.insert(std::make_pair("accept", "*/*"));
	test2.chunked_encoding = false;
	test2.body = "name=Mario";
	test2.clfd = -1;
	test_requests.push_back(test2);

	ClientRequest test3;
	test3.method = GET;
	test3.path = "cgi-bin/ciao.php";
	test3.query = "";
	test3.uri = test3.path;
	test3.version = "HTTP/1.1";
	test3.headers.clear();
	test3.headers.insert(std::make_pair("host", "localhost"));
	test3.headers.insert(std::make_pair("user-agent", "TestAgent/1.0"));
	test3.headers.insert(std::make_pair("accept", "*/*"));
	test3.chunked_encoding = false;
	test3.body = "";
	test3.clfd = -1;
	test_requests.push_back(test3);
}

int main(void) {
	init_requests();
	for (size_t i = 0; i < test_requests.size(); ++i) {
		CGIUtils::handle_CGI_request(test_requests[i], 0);
	}
	return (0);
}