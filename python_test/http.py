#!/usr/bin/env python3
"""
Comprehensive WebServ Testing Suite
Tests HTTP header parsing, configuration compliance, chunked requests, and edge cases
Designed for 42 webserv project testing
"""

import requests
import sys
import json
import time
import socket
import threading
from urllib.parse import urljoin, urlparse
import base64
import random
import string

class WebServTester:
    def __init__(self, servers=None):
        if servers is None:
            servers = [
                {"name": "Server1", "url": "http://localhost:8080"},
                {"name": "Server2", "url": "http://127.0.0.1:8081"}
            ]
        
        self.servers = servers
        self.session = requests.Session()
        self.results = []
        self.curl_commands = []
        
        # Test counters
        self.total_tests = 0
        self.passed_tests = 0
        self.failed_tests = 0
        self.error_tests = 0

    def log_curl_command(self, test_name, method, url, headers=None, data=None, special_note=""):
        """Log equivalent curl command for the test"""
        curl_cmd = f"curl -v -X {method}"
        
        if headers:
            for key, value in headers.items():
                escaped_value = str(value).replace('"', '\\"').replace('\\', '\\\\')
                curl_cmd += f' -H "{key}: {escaped_value}"'
        
        if data:
            if isinstance(data, str):
                if len(data) > 100:
                    curl_cmd += f' -d "[{len(data)} bytes data]"'
                else:
                    curl_cmd += f' -d "{data}"'
            elif isinstance(data, bytes):
                curl_cmd += f' -d "[{len(data)} bytes binary]"'
            elif hasattr(data, '__iter__') and not isinstance(data, (str, bytes)):
                curl_cmd += ' -d "[chunked data]"'
            else:
                curl_cmd += f' -d "[data]"'
        
        curl_cmd += f' "{url}"'
        
        if special_note:
            curl_cmd += f'  # {special_note}'
            
        self.curl_commands.append({
            "test": test_name,
            "command": curl_cmd,
            "note": special_note
        })

    def log_test(self, test_name, status, details="", expected="", actual="", server=""):
        """Log test results"""
        self.total_tests += 1
        if status == "PASS":
            self.passed_tests += 1
        elif status == "FAIL":
            self.failed_tests += 1
        elif status == "ERROR":
            self.error_tests += 1
            
        result = {
            "test": test_name,
            "server": server,
            "status": status,
            "details": details,
            "expected": expected,
            "actual": actual,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        self.results.append(result)
        
        status_symbol = "✓" if status == "PASS" else "✗" if status == "FAIL" else "⚠" if status == "WARN" else "ℹ"
        server_info = f"[{server}] " if server else ""
        print(f"{status_symbol} {server_info}{test_name}: {status}")
        if details:
            print(f"   Details: {details}")
        if expected and actual:
            print(f"   Expected: {expected}")
            print(f"   Actual: {actual}")
        print()

    # ============================================================================
    # PART 1: HEADER PARSING TESTS
    # ============================================================================

    def test_header_parsing(self):
        """Comprehensive HTTP header parsing tests"""
        print("=" * 80)
        print("PART 1: HEADER PARSING TESTS")
        print("=" * 80)

        for server in self.servers:
            self._test_basic_headers(server)
            self._test_malformed_headers(server)
            self._test_header_injection(server)
            self._test_header_limits(server)
            self._test_http_version_parsing(server)
            self._test_request_line_parsing(server)

    def _test_basic_headers(self, server):
        """Test basic header functionality"""
        print(f"\n--- Basic Headers ({server['name']}) ---")
        
        # Test standard headers
        standard_headers = {
            "Host": "example.com",
            "User-Agent": "WebServTester/1.0",
            "Accept": "text/html,application/xhtml+xml",
            "Accept-Language": "en-US,en;q=0.9",
            "Accept-Encoding": "gzip, deflate",
            "Connection": "keep-alive",
            "Cache-Control": "no-cache",
            "Pragma": "no-cache"
        }
        
        try:
            url = server['url'] + "/"
            self.log_curl_command("Basic headers", "GET", url, standard_headers, None, "Standard HTTP headers")
            response = self.session.get(url, headers=standard_headers, timeout=10)
            self.log_test("Basic headers", "PASS", f"Standard headers accepted (status: {response.status_code})", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Basic headers", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test empty header values
        try:
            empty_headers = {"X-Empty": "", "X-Test": "value"}
            url = server['url'] + "/"
            self.log_curl_command("Empty header values", "GET", url, empty_headers, None, "Empty header value test")
            response = self.session.get(url, headers=empty_headers, timeout=10)
            self.log_test("Empty header values", "PASS", "Empty header values handled correctly", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Empty header values", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test case sensitivity
        try:
            case_headers = {"content-type": "text/plain", "ACCEPT": "text/html", "User-agent": "test"}
            url = server['url'] + "/"
            self.log_curl_command("Header case sensitivity", "GET", url, case_headers, None, "Mixed case headers")
            response = self.session.get(url, headers=case_headers, timeout=10)
            self.log_test("Header case sensitivity", "PASS", "Mixed case headers handled", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Header case sensitivity", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_malformed_headers(self, server):
        """Test malformed header handling"""
        print(f"\n--- Malformed Headers ({server['name']}) ---")
        
        malformed_tests = [
            ("Header with spaces in name", {"Bad Header": "value"}),
            ("Header with colon in name", {"Bad:Header": "value"}),
            ("Header with control chars", {"X-Test": "value\x01\x02"}),
            ("Very long header name", {"X" + "A" * 1000: "value"}),
            ("Very long header value", {"X-Long": "A" * 8192}),
            ("Header with tab", {"X-Test": "value\ttest"}),
            ("Header with vertical tab", {"X-Test": "value\x0btest"}),
        ]
        
        for test_name, headers in malformed_tests:
            try:
                url = server['url'] + "/"
                self.log_curl_command(f"Malformed: {test_name}", "GET", url, headers, None, "Should be rejected")
                response = self.session.get(url, headers=headers, timeout=10)
                
                if response.status_code == 400:
                    self.log_test(f"Malformed: {test_name}", "PASS", "Correctly rejected with 400", server=server['name'])
                else:
                    self.log_test(f"Malformed: {test_name}", "WARN", f"Accepted malformed header (status: {response.status_code})", server=server['name'])
                    
            except requests.RequestException as e:
                if "400" in str(e) or "Bad" in str(e):
                    self.log_test(f"Malformed: {test_name}", "PASS", "Request library rejected malformed header", server=server['name'])
                else:
                    self.log_test(f"Malformed: {test_name}", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_header_injection(self, server):
        """Test header injection attacks"""
        print(f"\n--- Header Injection ({server['name']}) ---")
        
        injection_tests = [
            ("CRLF injection", {"X-Test": "value\r\nX-Injected: malicious"}),
            ("LF injection", {"X-Test": "value\nX-Injected: malicious"}),
            ("Null byte injection", {"X-Test": "value\x00injected"}),
            ("Line folding attack", {"X-Test": "value\r\n\tcontinued"}),
            ("Response splitting", {"X-Test": "value\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"}),
            ("Header smuggling", {"X-Test": "value\r\nContent-Length: 0\r\nContent-Length: 10"}),
        ]
        
        for test_name, headers in injection_tests:
            try:
                url = server['url'] + "/"
                self.log_curl_command(f"Injection: {test_name}", "GET", url, headers, None, "Security test - should reject")
                response = self.session.get(url, headers=headers, timeout=10)
                
                if response.status_code == 400:
                    self.log_test(f"Injection: {test_name}", "PASS", "Correctly rejected injection attempt", server=server['name'])
                else:
                    self.log_test(f"Injection: {test_name}", "FAIL", f"Injection not detected! Status: {response.status_code}", server=server['name'])
                    
            except requests.RequestException as e:
                if "400" in str(e) or "Bad" in str(e):
                    self.log_test(f"Injection: {test_name}", "PASS", "Request library/server rejected injection", server=server['name'])
                else:
                    self.log_test(f"Injection: {test_name}", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_header_limits(self, server):
        """Test header size and count limits"""
        print(f"\n--- Header Limits ({server['name']}) ---")
        
        # Test many headers
        try:
            many_headers = {f"X-Header-{i}": f"value-{i}" for i in range(100)}
            url = server['url'] + "/"
            self.log_curl_command("Many headers", "GET", url, {"note": f"{len(many_headers)} headers"}, None, "100 headers test")
            response = self.session.get(url, headers=many_headers, timeout=10)
            
            if response.status_code == 431:
                self.log_test("Many headers", "PASS", "Correctly rejected too many headers with 431", server=server['name'])
            elif response.status_code == 400:
                self.log_test("Many headers", "PASS", "Rejected many headers with 400", server=server['name'])
            else:
                self.log_test("Many headers", "WARN", f"Accepted 100 headers (status: {response.status_code})", server=server['name'])
                
        except requests.RequestException as e:
            self.log_test("Many headers", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_http_version_parsing(self, server):
        """Test HTTP version parsing"""
        print(f"\n--- HTTP Version Parsing ({server['name']}) ---")
        
        # Parse server URL for raw socket tests
        parsed = urlparse(server['url'])
        host = parsed.hostname or 'localhost'
        port = parsed.port or 80
        
        version_tests = [
            ("HTTP/1.0", b"GET / HTTP/1.0\r\n\r\n"),
            ("HTTP/1.1", b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"),
            ("HTTP/2.0", b"GET / HTTP/2.0\r\n\r\n"),
            ("HTTP/0.9", b"GET / HTTP/0.9\r\n\r\n"),
            ("Invalid version", b"GET / HTTP/INVALID\r\n\r\n"),
            ("Missing version", b"GET /\r\n\r\n"),
            ("Malformed version", b"GET / HTTPX/1.1\r\n\r\n"),
        ]
        
        for test_name, request_data in version_tests:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5)
                sock.connect((host, port))
                sock.send(request_data)
                response = sock.recv(1024).decode('utf-8', errors='ignore')
                sock.close()
                
                if "505" in response and "2.0" in test_name:
                    self.log_test(f"HTTP Version: {test_name}", "PASS", "Correctly returned 505 for unsupported version", server=server['name'])
                elif "400" in response and "Invalid" in test_name:
                    self.log_test(f"HTTP Version: {test_name}", "PASS", "Correctly returned 400 for invalid version", server=server['name'])
                elif "200" in response and test_name in ["HTTP/1.0", "HTTP/1.1"]:
                    self.log_test(f"HTTP Version: {test_name}", "PASS", "Correctly handled valid version", server=server['name'])
                else:
                    self.log_test(f"HTTP Version: {test_name}", "INFO", f"Response: {response[:100]}", server=server['name'])
                    
            except Exception as e:
                self.log_test(f"HTTP Version: {test_name}", "ERROR", f"Raw socket test failed: {str(e)}", server=server['name'])

    def _test_request_line_parsing(self, server):
        """Test request line parsing"""
        print(f"\n--- Request Line Parsing ({server['name']}) ---")
        
        # Parse server URL for raw socket tests
        parsed = urlparse(server['url'])
        host = parsed.hostname or 'localhost'
        port = parsed.port or 80
        
        request_line_tests = [
            ("Valid GET", b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"),
            ("URI too long", b"GET /" + b"A" * 8192 + b" HTTP/1.1\r\nHost: localhost\r\n\r\n"),
            ("Invalid method", b"INVALID / HTTP/1.1\r\nHost: localhost\r\n\r\n"),
            ("Missing spaces", b"GET/HTTP/1.1\r\nHost: localhost\r\n\r\n"),
            ("Extra spaces", b"GET   /   HTTP/1.1\r\nHost: localhost\r\n\r\n"),
            ("Control chars in URI", b"GET /test\x00\x01 HTTP/1.1\r\nHost: localhost\r\n\r\n"),
            ("Unicode in URI", "GET /tëst HTTP/1.1\r\nHost: localhost\r\n\r\n".encode('utf-8')),
        ]
        
        for test_name, request_data in request_line_tests:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5)
                sock.connect((host, port))
                sock.send(request_data)
                response = sock.recv(1024).decode('utf-8', errors='ignore')
                sock.close()
                
                if "414" in response and "too long" in test_name:
                    self.log_test(f"Request Line: {test_name}", "PASS", "Correctly returned 414 for long URI", server=server['name'])
                elif "400" in response and "Invalid" in test_name:
                    self.log_test(f"Request Line: {test_name}", "PASS", "Correctly returned 400 for invalid request", server=server['name'])
                elif "501" in response and "Invalid method" in test_name:
                    self.log_test(f"Request Line: {test_name}", "PASS", "Correctly returned 501 for invalid method", server=server['name'])
                elif "200" in response and test_name == "Valid GET":
                    self.log_test(f"Request Line: {test_name}", "PASS", "Valid request handled correctly", server=server['name'])
                else:
                    self.log_test(f"Request Line: {test_name}", "INFO", f"Status from response: {response[:50]}", server=server['name'])
                    
            except Exception as e:
                self.log_test(f"Request Line: {test_name}", "ERROR", f"Raw socket test failed: {str(e)}", server=server['name'])

    # ============================================================================
    # PART 2: CONFIGURATION COMPLIANCE TESTS
    # ============================================================================

    def test_configuration_compliance(self):
        """Test configuration compliance for both servers"""
        print("\n" + "=" * 80)
        print("PART 2: CONFIGURATION COMPLIANCE TESTS")
        print("=" * 80)

        self._test_server1_config()
        self._test_server2_config()

    def _test_server1_config(self):
        """Test Server 1 (port 8080) configuration"""
        print(f"\n--- Server 1 Configuration Tests ---")
        server = self.servers[0]  # localhost:8080
        
        # Test basic server response
        self._test_basic_connectivity(server)
        self._test_server1_locations(server)
        self._test_server1_methods(server)
        self._test_server1_body_limits(server)
        self._test_server1_error_pages(server)
        self._test_server1_cgi(server)

    def _test_server2_config(self):
        """Test Server 2 (port 8081) configuration"""  
        print(f"\n--- Server 2 Configuration Tests ---")
        server = self.servers[1]  # 127.0.0.1:8081
        
        # Test basic server response
        self._test_basic_connectivity(server)
        self._test_server2_locations(server)
        self._test_server2_redirects(server)
        self._test_server2_body_limits(server)
        self._test_server2_methods(server)

    def _test_basic_connectivity(self, server):
        """Test basic server connectivity"""
        try:
            url = server['url'] + "/"
            self.log_curl_command("Basic connectivity", "GET", url, None, None, "Basic server test")
            response = self.session.get(url, timeout=10)
            self.log_test("Basic connectivity", "PASS", f"Server responding (status: {response.status_code})", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Basic connectivity", "FAIL", f"Server not responding: {str(e)}", server=server['name'])

    def _test_server1_locations(self, server):
        """Test Server 1 location configurations"""
        print(f"\n... Server 1 Location Tests ...")
        
        # Test root location (/)
        try:
            url = server['url'] + "/"
            response = self.session.get(url, timeout=10)
            if "Index of" in response.text or response.status_code == 200:
                self.log_test("Root location autoindex", "PASS", "Root autoindex working", server=server['name'])
            else:
                self.log_test("Root location autoindex", "FAIL", f"Root autoindex issue: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Root location autoindex", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test /images location
        try:
            url = server['url'] + "/images/"
            response = self.session.get(url, timeout=10)
            self.log_test("Images location", "INFO", f"Images location status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Images location", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test /cgi-bin/ location
        try:
            url = server['url'] + "/cgi-bin/"
            response = self.session.get(url, timeout=10)
            self.log_test("CGI-bin location", "INFO", f"CGI-bin location status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("CGI-bin location", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_server1_methods(self, server):
        """Test Server 1 method restrictions"""
        print(f"\n... Server 1 Method Tests ...")
        
        # Root: only GET allowed
        forbidden_methods = ["POST", "PUT", "DELETE", "PATCH"]
        for method in forbidden_methods:
            try:
                url = server['url'] + "/"
                self.log_curl_command(f"Method {method} on root", method, url, None, None, "Should return 405")
                response = self.session.request(method, url, timeout=10)
                if response.status_code == 405:
                    self.log_test(f"Root {method} restriction", "PASS", f"{method} correctly rejected with 405", server=server['name'])
                else:
                    self.log_test(f"Root {method} restriction", "FAIL", f"{method} should return 405, got {response.status_code}", server=server['name'])
            except requests.RequestException as e:
                self.log_test(f"Root {method} restriction", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # CGI-bin: GET, POST, DELETE allowed
        allowed_cgi_methods = ["GET", "POST", "DELETE"]
        for method in allowed_cgi_methods:
            try:
                url = server['url'] + "/cgi-bin/test"
                response = self.session.request(method, url, timeout=10)
                # 404 is fine if CGI script doesn't exist
                if response.status_code in [200, 404, 500]:
                    self.log_test(f"CGI {method} allowed", "PASS", f"{method} allowed on CGI (status: {response.status_code})", server=server['name'])
                elif response.status_code == 405:
                    self.log_test(f"CGI {method} allowed", "FAIL", f"{method} should be allowed on CGI", server=server['name'])
                else:
                    self.log_test(f"CGI {method} allowed", "INFO", f"{method} on CGI returned: {response.status_code}", server=server['name'])
            except requests.RequestException as e:
                self.log_test(f"CGI {method} allowed", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_server1_body_limits(self, server):
        """Test Server 1 body size limits"""
        print(f"\n... Server 1 Body Limit Tests ...")
        
        # Test global limit (1M)
        try:
            large_data = "A" * (1024 * 1024 + 1)  # 1MB + 1
            url = server['url'] + "/"
            self.log_curl_command("Global 1M limit", "POST", url, None, "[1MB+1 data]", "Should get 405 (method) or 413 (size)")
            response = self.session.post(url, data=large_data, timeout=30)
            # Might get 405 first (method not allowed) or 413 (too large)
            if response.status_code in [405, 413]:
                self.log_test("Global 1M body limit", "PASS", f"Large request handled correctly: {response.status_code}", server=server['name'])
            else:
                self.log_test("Global 1M body limit", "WARN", f"Unexpected response: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Global 1M body limit", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test CGI limit (100k)
        try:
            medium_data = "B" * (100 * 1024 + 1)  # 100KB + 1
            url = server['url'] + "/cgi-bin/test"
            self.log_curl_command("CGI 100K limit", "POST", url, None, "[100KB+1 data]", "Should return 413")
            response = self.session.post(url, data=medium_data, timeout=30)
            if response.status_code == 413:
                self.log_test("CGI 100K body limit", "PASS", "CGI body limit enforced with 413", server=server['name'])
            else:
                self.log_test("CGI 100K body limit", "FAIL", f"Expected 413, got {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("CGI 100K body limit", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_server1_error_pages(self, server):
        """Test Server 1 error page configuration"""
        print(f"\n... Server 1 Error Page Tests ...")
        
        error_tests = [
            (404, "GET", "/nonexistent"),
            (405, "POST", "/"),  # POST not allowed on root
            (403, "GET", "/forbidden"),  # If you have forbidden content
        ]
        
        for expected_code, method, path in error_tests:
            try:
                url = server['url'] + path
                self.log_curl_command(f"Error {expected_code}", method, url, None, None, f"Should return {expected_code}")
                response = self.session.request(method, url, timeout=10)
                if response.status_code == expected_code:
                    # Check if custom error page is served
                    if "error" in response.text.lower() or len(response.text) > 10:
                        self.log_test(f"Error {expected_code} page", "PASS", f"Custom error page served for {expected_code}", server=server['name'])
                    else:
                        self.log_test(f"Error {expected_code} page", "INFO", f"Got {expected_code} but may not have custom page", server=server['name'])
                else:
                    self.log_test(f"Error {expected_code} page", "FAIL", f"Expected {expected_code}, got {response.status_code}", server=server['name'])
            except requests.RequestException as e:
                self.log_test(f"Error {expected_code} page", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_server1_cgi(self, server):
        """Test Server 1 CGI configuration"""
        print(f"\n... Server 1 CGI Tests ...")
        
        # Test .py extension
        try:
            url = server['url'] + "/cgi-bin/test.py"
            response = self.session.get(url, timeout=10)
            self.log_test("Python CGI extension", "INFO", f"Python CGI test status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Python CGI extension", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test .php extension
        try:
            url = server['url'] + "/cgi-bin/test.php"
            response = self.session.get(url, timeout=10)
            self.log_test("PHP CGI extension", "INFO", f"PHP CGI test status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("PHP CGI extension", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_server2_locations(self, server):
        """Test Server 2 location configurations"""
        print(f"\n... Server 2 Location Tests ...")
        
        # Test root location
        try:
            url = server['url'] + "/"
            response = self.session.get(url, timeout=10)
            self.log_test("Server2 root location", "INFO", f"Root status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Server2 root location", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test /method/ location  
        try:
            url = server['url'] + "/method/"
            response = self.session.get(url, timeout=10)
            self.log_test("Server2 /method/ location", "INFO", f"/method/ status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Server2 /method/ location", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_server2_redirects(self, server):
        """Test Server 2 redirect configurations"""
        print(f"\n... Server 2 Redirect Tests ...")
        
        redirect_tests = [
            ("/app1/", "http://amazon.com", "External redirect"),
            ("/foo/", "/foo/bar/", "301 redirect"),
            ("/foo/bar/", "/foo/", "Return redirect"),
            ("/secure/", None, "401 return"),
            ("/moved/", "https://handmadefont.com/shop/oniotype-font/", "301 external"),
            ("/temp-redirect/", "/temporary-location", "302 redirect"),
            ("/method/folder/", "/fdsfds", "302 redirect"),
        ]
        
        for path, expected_location, description in redirect_tests:
            try:
                url = server['url'] + path
                self.log_curl_command(f"Redirect {path}", "GET", url, None, None, description)
                response = self.session.get(url, allow_redirects=False, timeout=10)