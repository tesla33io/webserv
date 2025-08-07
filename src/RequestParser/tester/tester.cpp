/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tester.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 11:44:05 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 13:52:07 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../RequestParser.hpp"

std::vector<std::pair<std::string, std::string> > test_requests;

void init_requests() {
	test_requests.push_back(
	    std::make_pair("no_content_length (BAD)",
	                   "POST /submit HTTP/1.1\r\n"
	                   "Host: example.com\r\n"
	                   "Content-Type: application/x-www-form-urlencoded\r\n"
	                   "\r\n"
	                   "username=admin&pass=123"));

	test_requests.push_back(
	    std::make_pair("zero_content_length (GOOD)",
	                   "POST /submit HTTP/1.1\r\n"
	                   "Host: example.com\r\n"
	                   "Content-Type: application/x-www-form-urlencoded\r\n"
	                   "Content-Length: 0\r\n"
	                   "\r\n"));

	test_requests.push_back(
	    std::make_pair("body_size_mismatch (BAD)",
	                   "POST /submit HTTP/1.1\r\n"
	                   "Host: example.com\r\n"
	                   "Content-Type: application/x-www-form-urlencoded\r\n"
	                   "Content-Length: 25\r\n"
	                   "\r\n"
	                   "username=admin&pass=123"));

	test_requests.push_back(std::make_pair("spaces_in_headers (BAD)",
	                                       "GET /page HTTP/1.1\r\n"
	                                       "Host :   example.com  \r\n"
	                                       "User-Agent:  TestAgent\r\n"
	                                       "\r\n"));

	test_requests.push_back(std::make_pair("invalid_method (BAD)",
	                                       "FAKE /index HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));

	test_requests.push_back(std::make_pair("get_request (GOOD)",
	                                       "GET /index.html HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));

	test_requests.push_back(
	    std::make_pair("post_request (GOOD)",
	                   "POST /submit HTTP/1.1\r\n"
	                   "Host: example.com\r\n"
	                   "Content-Type: application/x-www-form-urlencoded\r\n"
	                   "Content-Length: 12\r\n"
	                   "\r\n"
	                   "name=ChatGPT"));
}

void add_malformed_request_line_tests() {
	// Missing HTTP version
	test_requests.push_back(std::make_pair("missing_http_version (BAD)",
	                                       "GET /index.html\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));

	// Invalid HTTP version
	test_requests.push_back(std::make_pair("invalid_http_version (BAD)",
	                                       "GET /index.html HTTP/2.0\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));

	// Missing URI
	test_requests.push_back(std::make_pair("missing_uri (BAD)",
	                                       "GET HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));

	// Extra spaces in request line
	test_requests.push_back(std::make_pair("extra_spaces_request_line (BAD)",
	                                       "GET  /index.html  HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));
}

void add_header_edge_cases() {
	// Missing Host header (required in HTTP/1.1)
	test_requests.push_back(std::make_pair("missing_host_header (BAD)",
	                                       "GET /index.html HTTP/1.1\r\n"
	                                       "User-Agent: TestAgent\r\n"
	                                       "\r\n"));

	// Empty header value (usually OK)
	test_requests.push_back(std::make_pair("empty_header_value (GOOD)",
	                                       "GET /index.html HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "X-Custom:\r\n"
	                                       "\r\n"));

	// Header with no colon
	test_requests.push_back(std::make_pair("header_no_colon (BAD)",
	                                       "GET /index.html HTTP/1.1\r\n"
	                                       "Host example.com\r\n"
	                                       "\r\n"));

	// Duplicate headers (depends on implementation)
	test_requests.push_back(std::make_pair("duplicate_headers (BAD)",
	                                       "GET /index.html HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "Host: another.com\r\n"
	                                       "\r\n"));

	// Very long header value
	test_requests.push_back(
	    std::make_pair("long_header_value (BAD)", "GET /index.html HTTP/1.1\r\n"
	                                              "Host: example.com\r\n"
	                                              "X-Long: " +
	                                                  std::string(8192, 'A') +
	                                                  "\r\n"
	                                                  "\r\n"));

	// Header folding (obsolete in HTTP/1.1 but might appear)
	test_requests.push_back(std::make_pair("header_folding (BAD)",
	                                       "GET /index.html HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "X-Folded: value1\r\n"
	                                       " continuation\r\n"
	                                       "\r\n"));
}

void add_line_ending_tests() {
	// Unix line endings (LF only)
	test_requests.push_back(std::make_pair("lf_only (BAD)",
	                                       "GET /index.html HTTP/1.1\n"
	                                       "Host: example.com\n"
	                                       "\n"));

	// Mixed line endings
	test_requests.push_back(std::make_pair("mixed_line_endings (BAD)",
	                                       "GET /index.html HTTP/1.1\r\n"
	                                       "Host: example.com\n"
	                                       "\r\n"));

	// Missing final CRLF
	test_requests.push_back(std::make_pair("missing_final_crlf (BAD)",
	                                       "GET /index.html HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"));
}

void add_content_length_edge_cases() {
	// Negative Content-Length
	test_requests.push_back(std::make_pair("negative_content_length (BAD)",
	                                       "POST /submit HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "Content-Length: -5\r\n"
	                                       "\r\n"));

	// Non-numeric Content-Length
	test_requests.push_back(std::make_pair("non_numeric_content_length (BAD)",
	                                       "POST /submit HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "Content-Length: abc\r\n"
	                                       "\r\n"));

	// Multiple Content-Length headers
	test_requests.push_back(std::make_pair("multiple_content_length (BAD)",
	                                       "POST /submit HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "Content-Length: 10\r\n"
	                                       "Content-Length: 15\r\n"
	                                       "\r\n"
	                                       "test=data"));

	// Very large Content-Length
	test_requests.push_back(std::make_pair("huge_content_length (BAD)",
	                                       "POST /submit HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "Content-Length: 999999999999999\r\n"
	                                       "\r\n"));
}

void add_uri_edge_cases() {
	// Empty URI
	test_requests.push_back(std::make_pair("empty_uri (BAD)",
	                                       "GET  HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));

	// URI with spaces (should be encoded)
	test_requests.push_back(std::make_pair("uri_with_spaces (BAD)",
	                                       "GET /path with spaces HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));

	// Very long URI
	test_requests.push_back(
	    std::make_pair("long_uri (BAD)", "GET /" + std::string(8192, 'a') +
	                                         " HTTP/1.1\r\n"
	                                         "Host: example.com\r\n"
	                                         "\r\n"));

	// URI with control characters
	test_requests.push_back(std::make_pair("uri_control_chars (BAD)",
	                                       "GET /path\x01\x02 HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));
}

void add_chunked_encoding_tests() {
	// Transfer-Encoding: chunked (OK if properly implemented)
	test_requests.push_back(std::make_pair("chunked_encoding (GOOD)",
	                                       "POST /submit HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "Transfer-Encoding: chunked\r\n"
	                                       "\r\n"
	                                       "5\r\n"
	                                       "hello\r\n"
	                                       "0\r\n"
	                                       "\r\n"));

	// Both Transfer-Encoding and Content-Length (should reject)
	test_requests.push_back(std::make_pair("chunked_with_content_length (BAD)",
	                                       "POST /submit HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "Transfer-Encoding: chunked\r\n"
	                                       "Content-Length: 10\r\n"
	                                       "\r\n"));
}

void add_case_sensitivity_tests() {
	// Mixed case method
	test_requests.push_back(std::make_pair("mixed_case_method (BAD)",
	                                       "Get /index.html HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));

	// Mixed case headers (should be case-insensitive - OK)
	test_requests.push_back(std::make_pair("mixed_case_headers (GOOD)",
	                                       "GET /index.html HTTP/1.1\r\n"
	                                       "HOST: example.com\r\n"
	                                       "content-type: text/html\r\n"
	                                       "\r\n"));
}

void add_malformed_body_tests() {
	// Body shorter than Content-Length (incomplete)
	test_requests.push_back(std::make_pair("incomplete_body (BAD)",
	                                       "POST /submit HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "Content-Length: 20\r\n"
	                                       "\r\n"
	                                       "short"));

	// Body with null bytes (usually OK but depends on implementation)
	std::string headers = "POST /submit HTTP/1.1\r\n"
	                      "Host: example.com\r\n"
	                      "Content-Length: 10\r\n"
	                      "\r\n";
	std::string body = std::string("test\x00data", 10);
	test_requests.push_back(
	    std::make_pair("body_with_nulls (GOOD)", headers + body));
}

void add_security_tests() {
	// Request smuggling attempt
	test_requests.push_back(std::make_pair("request_smuggling (BAD)",
	                                       "POST /submit HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "Content-Length: 6\r\n"
	                                       "Transfer-Encoding: chunked\r\n"
	                                       "\r\n"
	                                       "0\r\n"
	                                       "\r\n"
	                                       "GET /admin HTTP/1.1\r\n"
	                                       "Host: example.com\r\n"
	                                       "\r\n"));
}

void run_test() {
	for (size_t i = 0; i < test_requests.size(); ++i) {
		const std::string &name = test_requests[i].first;
		const std::string &raw = test_requests[i].second;

		ClientRequest request;
		bool ok = RequestParsingUtils::parseRequest(raw, request);

		std::cout << std::endl << "=== Test: " << name << " ===" << std::endl;
		std::cout << raw << std::endl;
		std::cout << "Result: " << (ok ? "PASS ✅" : "FAIL ❌") << std::endl;
		std::cout << "-----------------------------" << std::endl << std::endl;
	}
}

int main(void) {
	init_requests();
	add_malformed_request_line_tests();
	add_header_edge_cases();
	add_line_ending_tests();
	add_content_length_edge_cases();
	add_uri_edge_cases();
	add_chunked_encoding_tests();
	add_case_sensitivity_tests();
	add_malformed_body_tests();
	add_security_tests();
	run_test();
	return (0);
}
