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
                
                if path == "/secure/" and response.status_code == 401:
                    self.log_test(f"Redirect {path}", "PASS", "Correctly returned 401", server=server['name'])
                elif response.status_code in [301, 302]:
                    location = response.headers.get('Location', '')
                    if expected_location and expected_location in location:
                        self.log_test(f"Redirect {path}", "PASS", f"Correct redirect to {location}", server=server['name'])
                    elif expected_location:
                        self.log_test(f"Redirect {path}", "FAIL", f"Wrong redirect location", expected_location, location, server=server['name'])
                    else:
                        self.log_test(f"Redirect {path}", "INFO", f"Got redirect to {location}", server=server['name'])
                else:
                    self.log_test(f"Redirect {path}", "FAIL", f"Expected redirect, got {response.status_code}", server=server['name'])
                    
            except requests.RequestException as e:
                self.log_test(f"Redirect {path}", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_server2_body_limits(self, server):
        """Test Server 2 body size limits"""
        print(f"\n... Server 2 Body Limit Tests ...")
        
        # Test global limit (10k)
        try:
            large_data = "A" * (10 * 1024 + 1)  # 10KB + 1
            url = server['url'] + "/method/test/"  # Allows methods
            self.log_curl_command("Server2 10K limit", "DELETE", url, None, "[10KB+1 data]", "Should return 413")
            response = self.session.delete(url, data=large_data, timeout=30)
            if response.status_code == 413:
                self.log_test("Server2 10K body limit", "PASS", "10K body limit enforced with 413", server=server['name'])
            else:
                self.log_test("Server2 10K body limit", "FAIL", f"Expected 413, got {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Server2 10K body limit", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test root location 0k limit (no body allowed)
        try:
            small_data = "A"  # Even 1 byte should be rejected
            url = server['url'] + "/"
            response = self.session.post(url, data=small_data, timeout=10)
            if response.status_code == 413:
                self.log_test("Server2 0K body limit", "PASS", "0K body limit enforced - no body allowed", server=server['name'])
            elif response.status_code == 405:
                self.log_test("Server2 0K body limit", "INFO", "POST not allowed on root (405)", server=server['name'])
            else:
                self.log_test("Server2 0K body limit", "WARN", f"0K limit not enforced? Got {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Server2 0K body limit", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_server2_methods(self, server):
        """Test Server 2 method restrictions"""
        print(f"\n... Server 2 Method Tests ...")
        
        # Test /method/ - only GET allowed
        forbidden_methods = ["POST", "PUT", "DELETE", "PATCH"]
        for method in forbidden_methods:
            try:
                url = server['url'] + "/method/"
                response = self.session.request(method, url, timeout=10)
                if response.status_code == 405:
                    self.log_test(f"/method/ {method} restriction", "PASS", f"{method} correctly rejected with 405", server=server['name'])
                else:
                    self.log_test(f"/method/ {method} restriction", "FAIL", f"{method} should return 405, got {response.status_code}", server=server['name'])
            except requests.RequestException as e:
                self.log_test(f"/method/ {method} restriction", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test /method/test/ - only DELETE allowed
        other_methods = ["GET", "POST", "PUT", "PATCH"]
        for method in other_methods:
            try:
                url = server['url'] + "/method/test/"
                response = self.session.request(method, url, timeout=10)
                if response.status_code == 405:
                    self.log_test(f"/method/test/ {method} restriction", "PASS", f"{method} correctly rejected with 405", server=server['name'])
                else:
                    self.log_test(f"/method/test/ {method} restriction", "FAIL", f"{method} should return 405, got {response.status_code}", server=server['name'])
            except requests.RequestException as e:
                self.log_test(f"/method/test/ {method} restriction", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test DELETE is allowed on /method/test/
        try:
            url = server['url'] + "/method/test/"
            response = self.session.delete(url, timeout=10)
            if response.status_code in [200, 204, 404]:  # Success codes or not found
                self.log_test("/method/test/ DELETE allowed", "PASS", f"DELETE allowed (status: {response.status_code})", server=server['name'])
            elif response.status_code == 405:
                self.log_test("/method/test/ DELETE allowed", "FAIL", "DELETE should be allowed", server=server['name'])
            else:
                self.log_test("/method/test/ DELETE allowed", "INFO", f"DELETE returned: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("/method/test/ DELETE allowed", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    # ============================================================================
    # PART 3: CHUNKED TRANSFER ENCODING TESTS
    # ============================================================================

    def test_chunked_requests(self):
        """Comprehensive chunked transfer encoding tests"""
        print("\n" + "=" * 80)
        print("PART 3: CHUNKED TRANSFER ENCODING TESTS")
        print("=" * 80)

        for server in self.servers:
            self._test_basic_chunked(server)
            self._test_chunked_edge_cases(server)
            self._test_chunked_limits(server)
            self._test_chunked_malformed(server)

    def _test_basic_chunked(self, server):
        """Test basic chunked functionality"""
        print(f"\n--- Basic Chunked Tests ({server['name']}) ---")
        
        # Test 1: Simple chunked request
        try:
            data = "Hello World from chunks!"
            chunks = [data.encode()]
            url = server['url'] + "/cgi-bin/test" if server['name'] == 'Server1' else server['url'] + "/method/test/"
            method = "POST" if server['name'] == 'Server1' else "DELETE"
            
            self.log_curl_command("Basic chunked", method, url, {"Transfer-Encoding": "chunked"}, data, "Basic chunked transfer")
            response = self.session.request(method, url, data=iter(chunks), timeout=10)
            self.log_test("Basic chunked", "INFO", f"Basic chunked status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Basic chunked", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test 2: Multiple small chunks
        try:
            chunks = ["Hello", " ", "World", " ", "from", " ", "multiple", " ", "chunks!"]
            url = server['url'] + "/cgi-bin/test" if server['name'] == 'Server1' else server['url'] + "/method/test/"
            method = "POST" if server['name'] == 'Server1' else "DELETE"
            combined_data = "".join(chunks)
            
            self.log_curl_command("Multiple chunks", method, url, {"Transfer-Encoding": "chunked"}, combined_data, "Multiple small chunks")
            response = self.session.request(method, url, data=iter(chunk.encode() for chunk in chunks), timeout=10)
            self.log_test("Multiple chunks", "INFO", f"Multiple chunks status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Multiple chunks", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test 3: Binary chunked data
        try:
            binary_chunks = [b'\x00\x01\x02\x03', b'\x04\x05\x06\x07', b'\x08\x09\x0a\x0b']
            url = server['url'] + "/cgi-bin/test" if server['name'] == 'Server1' else server['url'] + "/method/test/"
            method = "POST" if server['name'] == 'Server1' else "DELETE"
            
            self.log_curl_command("Binary chunks", method, url, 
                                {"Transfer-Encoding": "chunked", "Content-Type": "application/octet-stream"}, 
                                "[binary data]", "Binary chunked data")
            response = self.session.request(method, url, data=iter(binary_chunks), 
                                          headers={"Content-Type": "application/octet-stream"}, timeout=10)
            self.log_test("Binary chunks", "INFO", f"Binary chunks status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Binary chunks", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_chunked_edge_cases(self, server):
        """Test chunked edge cases"""
        print(f"\n--- Chunked Edge Cases ({server['name']}) ---")
        
        # Test 1: Empty chunks
        try:
            chunks = ["", "Hello", "", "World", ""]
            url = server['url'] + "/cgi-bin/test" if server['name'] == 'Server1' else server['url'] + "/method/test/"
            method = "POST" if server['name'] == 'Server1' else "DELETE"
            
            self.log_curl_command("Empty chunks", method, url, {"Transfer-Encoding": "chunked"}, "HelloWorld", "Contains empty chunks")
            response = self.session.request(method, url, data=iter(chunk.encode() for chunk in chunks), timeout=10)
            self.log_test("Empty chunks", "INFO", f"Empty chunks status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Empty chunks", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test 2: Single byte chunks
        try:
            message = "ABCDEFGHIJ"
            chunks = [char.encode() for char in message]
            url = server['url'] + "/cgi-bin/test" if server['name'] == 'Server1' else server['url'] + "/method/test/"
            method = "POST" if server['name'] == 'Server1' else "DELETE"
            
            self.log_curl_command("Single byte chunks", method, url, {"Transfer-Encoding": "chunked"}, message, "Each character as separate chunk")
            response = self.session.request(method, url, data=iter(chunks), timeout=10)
            self.log_test("Single byte chunks", "INFO", f"Single byte chunks status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Single byte chunks", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        # Test 3: Many small chunks (stress test)
        try:
            chunks = [f"chunk{i:03d}".encode() for i in range(50)]
            url = server['url'] + "/cgi-bin/test" if server['name'] == 'Server1' else server['url'] + "/method/test/"
            method = "POST" if server['name'] == 'Server1' else "DELETE"
            
            self.log_curl_command("Many chunks stress", method, url, {"Transfer-Encoding": "chunked"}, "[50 chunks]", "50 chunk stress test")
            response = self.session.request(method, url, data=iter(chunks), timeout=30)
            self.log_test("Many chunks stress", "INFO", f"50 chunks stress status: {response.status_code}", server=server['name'])
        except requests.RequestException as e:
            self.log_test("Many chunks stress", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_chunked_limits(self, server):
        """Test chunked transfer with body size limits"""
        print(f"\n--- Chunked Limits ({server['name']}) ---")
        
        if server['name'] == 'Server1':
            # Test CGI limit (100K) with chunks
            try:
                # Create chunks that total just over 100KB
                chunk_data = "A" * 1024  # 1KB per chunk
                chunks = [chunk_data.encode() for _ in range(101)]  # 101KB total
                url = server['url'] + "/cgi-bin/test"
                
                self.log_curl_command("Chunked CGI limit", "POST", url, {"Transfer-Encoding": "chunked"}, "[101KB chunked]", "Should return 413")
                response = self.session.post(url, data=iter(chunks), timeout=30)
                
                if response.status_code == 413:
                    self.log_test("Chunked CGI limit", "PASS", "Chunked request correctly rejected with 413", server=server['name'])
                else:
                    self.log_test("Chunked CGI limit", "FAIL", f"Expected 413, got {response.status_code}", server=server['name'])
            except requests.RequestException as e:
                self.log_test("Chunked CGI limit", "ERROR", f"Request failed: {str(e)}", server=server['name'])

            # Test global limit (1M) with chunks
            try:
                # Create chunks that total just over 1MB
                chunk_data = "B" * (10 * 1024)  # 10KB per chunk
                chunks = [chunk_data.encode() for _ in range(105)]  # ~1.05MB total
                url = server['url'] + "/"
                
                self.log_curl_command("Chunked global limit", "POST", url, {"Transfer-Encoding": "chunked"}, "[1.05MB chunked]", "Should return 405 or 413")
                response = self.session.post(url, data=iter(chunks), timeout=30)
                
                if response.status_code in [405, 413]:
                    self.log_test("Chunked global limit", "PASS", f"Large chunked request handled: {response.status_code}", server=server['name'])
                else:
                    self.log_test("Chunked global limit", "WARN", f"Unexpected status: {response.status_code}", server=server['name'])
            except requests.RequestException as e:
                self.log_test("Chunked global limit", "ERROR", f"Request failed: {str(e)}", server=server['name'])

        else:  # Server2
            # Test Server2 limits with chunks
            try:
                # Create chunks that total just over 10KB
                chunk_data = "C" * 512  # 512 bytes per chunk
                chunks = [chunk_data.encode() for _ in range(21)]  # ~10.5KB total
                url = server['url'] + "/method/test/"
                
                self.log_curl_command("Chunked Server2 limit", "DELETE", url, {"Transfer-Encoding": "chunked"}, "[10.5KB chunked]", "Should return 413")
                response = self.session.delete(url, data=iter(chunks), timeout=30)
                
                if response.status_code == 413:
                    self.log_test("Chunked Server2 limit", "PASS", "Chunked request correctly rejected with 413", server=server['name'])
                else:
                    self.log_test("Chunked Server2 limit", "FAIL", f"Expected 413, got {response.status_code}", server=server['name'])
            except requests.RequestException as e:
                self.log_test("Chunked Server2 limit", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_chunked_malformed(self, server):
        """Test malformed chunked requests"""
        print(f"\n--- Malformed Chunked ({server['name']}) ---")
        
        # Parse server URL for raw socket tests
        parsed = urlparse(server['url'])
        host = parsed.hostname or 'localhost'
        port = parsed.port or 80
        
        malformed_chunked_tests = [
            ("Invalid chunk size", b"POST /test HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nINVALID\r\ndata\r\n0\r\n\r\n"),
            ("Missing final chunk", b"POST /test HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n"),
            ("Negative chunk size", b"POST /test HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n-5\r\ndata\r\n0\r\n\r\n"),
            ("Chunk size too large", b"POST /test HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFFFF\r\ndata\r\n0\r\n\r\n"),
            ("Missing CRLF after chunk", b"POST /test HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhelloX0\r\n\r\n"),
        ]
        
        for test_name, request_data in malformed_chunked_tests:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5)
                sock.connect((host, port))
                sock.send(request_data)
                response = sock.recv(1024).decode('utf-8', errors='ignore')
                sock.close()
                
                if "400" in response:
                    self.log_test(f"Malformed chunked: {test_name}", "PASS", "Correctly rejected malformed chunked with 400", server=server['name'])
                else:
                    self.log_test(f"Malformed chunked: {test_name}", "FAIL", f"Should reject malformed chunked, got: {response[:50]}", server=server['name'])
                    
            except Exception as e:
                self.log_test(f"Malformed chunked: {test_name}", "ERROR", f"Raw socket test failed: {str(e)}", server=server['name'])

    # ============================================================================
    # PART 4: EDGE CASES AND STRESS TESTS
    # ============================================================================

    def test_edge_cases_and_stress(self):
        """Test edge cases, stress conditions, and advanced scenarios"""
        print("\n" + "=" * 80)
        print("PART 4: EDGE CASES AND STRESS TESTS")
        print("=" * 80)

        for server in self.servers:
            self._test_concurrent_requests(server)
            self._test_slow_requests(server)
            self._test_connection_handling(server)
            self._test_uri_edge_cases(server)
            self._test_content_types(server)
            self._test_encoding_edge_cases(server)

    def _test_concurrent_requests(self, server):
        """Test concurrent request handling"""
        print(f"\n--- Concurrent Requests ({server['name']}) ---")
        
        def make_request(url, delay=0):
            try:
                time.sleep(delay)
                response = requests.get(url, timeout=10)
                return response.status_code
            except:
                return None

        try:
            url = server['url'] + "/"
            
            # Create 10 concurrent requests
            threads = []
            results = []
            
            for i in range(10):
                thread = threading.Thread(target=lambda: results.append(make_request(url, i * 0.1)))
                threads.append(thread)
                thread.start()
            
            # Wait for all threads
            for thread in threads:
                thread.join()
            
            successful = [r for r in results if r is not None and r < 500]
            self.log_test("Concurrent requests", "PASS" if len(successful) >= 8 else "WARN", 
                         f"Handled {len(successful)}/10 concurrent requests", server=server['name'])
                         
        except Exception as e:
            self.log_test("Concurrent requests", "ERROR", f"Concurrent test failed: {str(e)}", server=server['name'])

    def _test_slow_requests(self, server):
        """Test slow client scenarios"""
        print(f"\n--- Slow Request Tests ({server['name']}) ---")
        
        # Parse server URL for raw socket tests
        parsed = urlparse(server['url'])
        host = parsed.hostname or 'localhost'
        port = parsed.port or 80
        
        # Test slow headers (send headers very slowly)
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)
            sock.connect((host, port))
            
            # Send request line
            sock.send(b"GET / HTTP/1.1\r\n")
            time.sleep(0.5)
            
            # Send headers slowly
            headers = [
                b"Host: localhost\r\n",
                b"User-Agent: SlowClient\r\n", 
                b"Accept: text/html\r\n",
                b"\r\n"
            ]
            
            for header in headers:
                sock.send(header)
                time.sleep(0.5)
            
            response = sock.recv(1024).decode('utf-8', errors='ignore')
            sock.close()
            
            if "200" in response or "404" in response:
                self.log_test("Slow headers", "PASS", "Server handled slow headers correctly", server=server['name'])
            elif "400" in response or "408" in response:
                self.log_test("Slow headers", "INFO", "Server rejected slow headers (timeout/bad request)", server=server['name'])
            else:
                self.log_test("Slow headers", "WARN", f"Unexpected response to slow headers: {response[:50]}", server=server['name'])
                
        except Exception as e:
            self.log_test("Slow headers", "ERROR", f"Slow headers test failed: {str(e)}", server=server['name'])

    def _test_connection_handling(self, server):
        """Test connection handling edge cases"""
        print(f"\n--- Connection Handling ({server['name']}) ---")
        
        parsed = urlparse(server['url'])
        host = parsed.hostname or 'localhost'
        port = parsed.port or 80
        
        # Test incomplete request (connection drop)
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((host, port))
            sock.send(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
            # Don't send final \r\n and close connection
            sock.close()
            self.log_test("Incomplete request", "PASS", "Connection closed cleanly for incomplete request", server=server['name'])
        except Exception as e:
            self.log_test("Incomplete request", "ERROR", f"Incomplete request test failed: {str(e)}", server=server['name'])

        # Test connection reuse
        try:
            with requests.Session() as session:
                url = server['url'] + "/"
                resp1 = session.get(url, timeout=10)
                resp2 = session.get(url, timeout=10)
                
                if resp1.status_code < 500 and resp2.status_code < 500:
                    self.log_test("Connection reuse", "PASS", "Connection reuse working", server=server['name'])
                else:
                    self.log_test("Connection reuse", "WARN", "Connection reuse issues", server=server['name'])
        except Exception as e:
            self.log_test("Connection reuse", "ERROR", f"Connection reuse test failed: {str(e)}", server=server['name'])

    def _test_uri_edge_cases(self, server):
        """Test URI parsing edge cases"""
        print(f"\n--- URI Edge Cases ({server['name']}) ---")
        
        uri_tests = [
            ("/", "Root path"),
            ("//", "Double slash"),
            ("/./", "Current directory"),
            ("/../", "Parent directory"),
            ("/path/../", "Path with parent"),
            ("/path/./file", "Path with current"),
            ("/path%20with%20spaces", "URL encoded spaces"),
            ("/path?query=value", "Query string"),
            ("/path?query=value&other=test", "Multiple query params"),
            ("/path#fragment", "Fragment"),
            ("/path%2F%2E%2E%2F", "Encoded path traversal"),
            ("/%00", "Null byte in path"),
            ("/very/long/path/" + "segment/" * 20, "Very long path"),
        ]
        
        for uri, description in uri_tests:
            try:
                url = server['url'] + uri
                self.log_curl_command(f"URI: {description}", "GET", url, None, None, description)
                response = self.session.get(url, timeout=10)
                
                if uri == "/%00" and response.status_code == 400:
                    self.log_test(f"URI: {description}", "PASS", "Correctly rejected null byte", server=server['name'])
                elif "/.." in uri and response.status_code in [400, 403]:
                    self.log_test(f"URI: {description}", "PASS", "Path traversal blocked", server=server['name'])
                elif len(uri) > 200 and response.status_code == 414:
                    self.log_test(f"URI: {description}", "PASS", "Long URI rejected with 414", server=server['name'])
                else:
                    self.log_test(f"URI: {description}", "INFO", f"URI handled with status: {response.status_code}", server=server['name'])
                    
            except requests.RequestException as e:
                self.log_test(f"URI: {description}", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_content_types(self, server):
        """Test various content types"""
        print(f"\n--- Content Types ({server['name']}) ---")
        
        content_types = [
            ("application/json", '{"key": "value"}'),
            ("application/xml", '<?xml version="1.0"?><root><item>test</item></root>'),
            ("application/octet-stream", b'\x00\x01\x02\x03\x04'),
            ("multipart/form-data; boundary=test", '--test\r\nContent-Disposition: form-data; name="field"\r\n\r\nvalue\r\n--test--'),
            ("application/x-www-form-urlencoded", "field1=value1&field2=value2"),
            ("text/plain; charset=utf-8", "Hello, 世界! 🌍"),
            ("image/jpeg", b'\xff\xd8\xff\xe0\x00\x10JFIF'),
        ]
        
        for content_type, data in content_types:
            try:
                # Use appropriate endpoint for each server
                if server['name'] == 'Server1':
                    url = server['url'] + "/cgi-bin/test"
                    method = "POST"
                else:
                    url = server['url'] + "/method/test/"
                    method = "DELETE"
                
                headers = {"Content-Type": content_type}
                self.log_curl_command(f"Content-Type: {content_type}", method, url, headers, 
                                    data if isinstance(data, str) else f"[{len(data)} bytes]", f"Test {content_type}")
                
                if isinstance(data, str):
                    response = self.session.request(method, url, data=data, headers=headers, timeout=10)
                else:
                    response = self.session.request(method, url, data=data, headers=headers, timeout=10)
                    
                self.log_test(f"Content-Type: {content_type}", "INFO", 
                             f"Content type handled with status: {response.status_code}", server=server['name'])
                             
            except requests.RequestException as e:
                self.log_test(f"Content-Type: {content_type}", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    def _test_encoding_edge_cases(self, server):
        """Test character encoding edge cases"""
        print(f"\n--- Encoding Edge Cases ({server['name']}) ---")
        
        encoding_tests = [
            ("UTF-8", "Hello, 世界! 🌍🚀", "utf-8"),
            ("Latin-1", "Café résumé naïve", "latin-1"),
            ("ASCII", "Hello World 123", "ascii"),
            ("Mixed bytes", b'\x80\x81\x82\x83', None),
            ("Very long UTF-8", "🌍" * 1000, "utf-8"),
        ]
        
        for test_name, data, encoding in encoding_tests:
            try:
                if server['name'] == 'Server1':
                    url = server['url'] + "/cgi-bin/test"
                    method = "POST"
                else:
                    url = server['url'] + "/method/test/"
                    method = "DELETE"
                
                if encoding:
                    headers = {"Content-Type": f"text/plain; charset={encoding}"}
                    encoded_data = data.encode(encoding) if isinstance(data, str) else data
                else:
                    headers = {"Content-Type": "application/octet-stream"}
                    encoded_data = data
                
                self.log_curl_command(f"Encoding: {test_name}", method, url, headers, 
                                    f"[{len(encoded_data)} bytes {encoding or 'binary'}]", f"Test {test_name}")
                
                response = self.session.request(method, url, data=encoded_data, headers=headers, timeout=10)
                self.log_test(f"Encoding: {test_name}", "INFO", 
                             f"Encoding handled with status: {response.status_code}", server=server['name'])
                             
            except UnicodeError as e:
                self.log_test(f"Encoding: {test_name}", "ERROR", f"Encoding error: {str(e)}", server=server['name'])
            except requests.RequestException as e:
                self.log_test(f"Encoding: {test_name}", "ERROR", f"Request failed: {str(e)}", server=server['name'])

    # ============================================================================
    # UTILITY AND REPORTING METHODS
    # ============================================================================

    def run_all_tests(self):
        """Run all test suites"""
        print("WebServ Comprehensive Testing Suite")
        print("=" * 80)
        print(f"Testing servers: {[s['name'] + ' (' + s['url'] + ')' for s in self.servers]}")
        print(f"Test started at: {time.strftime('%Y-%m-%d %H:%M:%S')}")
        print()

        start_time = time.time()

        # Run all test parts
        self.test_header_parsing()
        self.test_configuration_compliance() 
        self.test_chunked_requests()
        self.test_edge_cases_and_stress()

        end_time = time.time()
        duration = end_time - start_time

        self.print_final_summary(duration)

    def print_final_summary(self, duration):
        """Print comprehensive test summary"""
        print("\n" + "=" * 80)
        print("FINAL TEST SUMMARY")
        print("=" * 80)

        # Overall statistics
        print(f"Total execution time: {duration:.2f} seconds")
        print(f"Total tests executed: {self.total_tests}")
        print(f"✓ PASSED: {self.passed_tests}")
        print(f"✗ FAILED: {self.failed_tests}")
        print(f"⚠ WARNINGS: {sum(1 for r in self.results if r['status'] == 'WARN')}")
        print(f"ℹ INFO: {sum(1 for r in self.results if r['status'] == 'INFO')}")
        print(f"💥 ERRORS: {self.error_tests}")
        
        success_rate = (self.passed_tests / self.total_tests * 100) if self.total_tests > 0 else 0
        print(f"Success rate: {success_rate:.1f}%")

        # Summary by server
        print(f"\n--- Summary by Server ---")
        for server in self.servers:
            server_results = [r for r in self.results if r.get('server') == server['name']]
            server_passed = sum(1 for r in server_results if r['status'] == 'PASS')
            server_failed = sum(1 for r in server_results if r['status'] == 'FAIL')
            server_total = len(server_results)
            
            if server_total > 0:
                server_rate = (server_passed / server_total * 100)
                print(f"{server['name']} ({server['url']}): {server_passed}/{server_total} passed ({server_rate:.1f}%)")

        # Summary by test category
        print(f"\n--- Summary by Test Category ---")
        categories = {}
        for result in self.results:
            # Extract category from test name (everything before first colon or first space)
            test_name = result['test']
            if ':' in test_name:
                category = test_name.split(':')[0]
            elif ' ' in test_name:
                category = test_name.split()[0]
            else:
                category = "Other"
            
            if category not in categories:
                categories[category] = {'total': 0, 'passed': 0, 'failed': 0}
            
            categories[category]['total'] += 1
            if result['status'] == 'PASS':
                categories[category]['passed'] += 1
            elif result['status'] == 'FAIL':
                categories[category]['failed'] += 1

        for category, stats in sorted(categories.items()):
            if stats['total'] > 0:
                rate = (stats['passed'] / stats['total'] * 100)
                print(f"{category}: {stats['passed']}/{stats['total']} passed ({rate:.1f}%)")

        # Critical failures
        critical_failures = [r for r in self.results if r['status'] == 'FAIL' and 
                           any(keyword in r['test'].lower() for keyword in 
                               ['injection', 'security', 'malformed', 'limit'])]
        
        if critical_failures:
            print(f"\n--- ⚠️  CRITICAL SECURITY ISSUES ---")
            for result in critical_failures:
                print(f"❌ {result['test']}: {result['details']}")

        # Failed tests detail
        failed_results = [r for r in self.results if r['status'] == 'FAIL']
        if failed_results:
            print(f"\n--- ❌ FAILED TESTS ---")
            for result in failed_results:
                server_info = f"[{result.get('server', 'Unknown')}] " if result.get('server') else ""
                print(f"• {server_info}{result['test']}: {result['details']}")
                if result.get('expected') and result.get('actual'):
                    print(f"  Expected: {result['expected']}, Got: {result['actual']}")

        # Recommendations
        print(f"\n--- 📋 RECOMMENDATIONS ---")
        
        if self.failed_tests == 0:
            print("🎉 All tests passed! Your webserver implementation looks solid.")
        else:
            print("🔧 Focus on fixing the failed tests, especially:")
            
            if any('injection' in r['test'].lower() for r in failed_results):
                print("   • Header injection vulnerabilities - critical security issue")
            
            if any('malformed' in r['test'].lower() for r in failed_results):
                print("   • HTTP parsing robustness - improve malformed input handling")
            
            if any('limit' in r['test'].lower() for r in failed_results):
                print("   • Body size limits - ensure proper enforcement")
            
            if any('chunked' in r['test'].lower() for r in failed_results):
                print("   • Chunked transfer encoding - fix chunked request parsing")
            
            if any('method' in r['test'].lower() for r in failed_results):
                print("   • HTTP method restrictions - verify configuration compliance")

        if self.error_tests > 5:
            print("   • High error rate suggests connectivity or basic functionality issues")

        print(f"\n--- 📁 Output Files ---")
        print("• test_results.json - Detailed results in JSON format")
        print("• curl_commands.txt - Equivalent curl commands for manual testing")
        print("• Use curl commands to reproduce specific test scenarios manually")

    def save_results(self, filename="python_test/test_results.json"):
        """Save comprehensive test results to JSON file"""
        output_data = {
            "test_info": {
                "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                "servers_tested": self.servers,
                "total_tests": self.total_tests,
                "passed_tests": self.passed_tests,
                "failed_tests": self.failed_tests,
                "error_tests": self.error_tests,
                "success_rate": (self.passed_tests / self.total_tests * 100) if self.total_tests > 0 else 0
            },
            "test_results": self.results
        }
        
        with open(filename, 'w') as f:
            json.dump(output_data, f, indent=2)
        print(f"✅ Test results saved to {filename}")

    def save_curl_commands(self, filename="python_test/curl_commands.txt"):
        """Save equivalent curl commands to text file"""
        with open(filename, 'w') as f:
            f.write("# WebServ Comprehensive Testing - Equivalent Curl Commands\n")
            f.write("# =========================================================\n")
            f.write(f"# Generated at: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"# Servers tested: {', '.join([s['name'] + ' (' + s['url'] + ')' for s in self.servers])}\n")
            f.write(f"# Total commands: {len(self.curl_commands)}\n")
            f.write("#\n# Usage: Copy and paste commands to test manually\n")
            f.write("# Note: Some tests require raw socket connections and cannot be replicated with curl\n\n")
            
            current_category = ""
            for cmd in self.curl_commands:
                # Group commands by test category
                if ":" in cmd["test"]:
                    test_category = cmd["test"].split(":")[0]
                elif " " in cmd["test"]:
                    test_category = cmd["test"].split()[0]
                else:
                    test_category = "Other"
                
                if test_category != current_category:
                    f.write(f"\n# ===============================\n")
                    f.write(f"# {test_category.upper()} TESTS\n")
                    f.write(f"# ===============================\n\n")
                    current_category = test_category
                
                f.write(f"# Test: {cmd['test']}\n")
                if cmd["note"]:
                    f.write(f"# Note: {cmd['note']}\n")
                f.write(f"{cmd['command']}\n")
                f.write(f"# Expected: Check status code and response\n\n")
        
        print(f"✅ Curl commands saved to {filename}")

    def save_summary_report(self, filename="python_test/test_summary.md"):
        """Save a markdown summary report"""
        with open(filename, 'w') as f:
            f.write("# WebServ Testing Summary Report\n\n")
            f.write(f"**Generated:** {time.strftime('%Y-%m-%d %H:%M:%S')}  \n")
            f.write(f"**Servers Tested:** {len(self.servers)}  \n")
            f.write(f"**Total Tests:** {self.total_tests}  \n")
            f.write(f"**Success Rate:** {(self.passed_tests / self.total_tests * 100) if self.total_tests > 0 else 0:.1f}%\n\n")

            f.write("## Test Results Overview\n\n")
            f.write("| Status | Count | Percentage |\n")
            f.write("|--------|-------|------------|\n")
            f.write(f"| ✅ PASS | {self.passed_tests} | {(self.passed_tests / self.total_tests * 100) if self.total_tests > 0 else 0:.1f}% |\n")
            f.write(f"| ❌ FAIL | {self.failed_tests} | {(self.failed_tests / self.total_tests * 100) if self.total_tests > 0 else 0:.1f}% |\n")
            f.write(f"| ⚠️ ERROR | {self.error_tests} | {(self.error_tests / self.total_tests * 100) if self.total_tests > 0 else 0:.1f}% |\n\n")

            # Failed tests
            failed_results = [r for r in self.results if r['status'] == 'FAIL']
            if failed_results:
                f.write("## ❌ Failed Tests\n\n")
                for result in failed_results:
                    f.write(f"- **{result['test']}** ({result.get('server', 'Unknown')}): {result['details']}\n")
                f.write("\n")

            # Critical issues
            critical_failures = [r for r in self.results if r['status'] == 'FAIL' and 
                               any(keyword in r['test'].lower() for keyword in 
                                   ['injection', 'security', 'malformed', 'limit'])]
            if critical_failures:
                f.write("## 🚨 Critical Security Issues\n\n")
                for result in critical_failures:
                    f.write(f"- **{result['test']}**: {result['details']}\n")
                f.write("\n")

            f.write("## 📊 Test Categories\n\n")
            f.write("1. **Header Parsing Tests** - HTTP header validation and security\n")
            f.write("2. **Configuration Compliance** - Server configuration adherence\n") 
            f.write("3. **Chunked Transfer Encoding** - Chunked request handling\n")
            f.write("4. **Edge Cases & Stress Tests** - Robustness and performance\n\n")

            f.write("## 📁 Additional Files\n\n")
            f.write("- `test_results.json` - Complete test results in JSON format\n")
            f.write("- `curl_commands.txt` - Equivalent curl commands for manual testing\n")
            
        print(f"✅ Summary report saved to {filename}")


def main():
    """Main function"""
    # Parse command line arguments
    if len(sys.argv) > 1:
        if sys.argv[1] == "--help" or sys.argv[1] == "-h":
            print("WebServ Comprehensive Testing Suite")
            print("Usage: python3 webserv_tester.py [server1_url] [server2_url]")
            print("")
            print("Examples:")
            print("  python3 webserv_tester.py")
            print("  python3 webserv_tester.py http://localhost:8080")
            print("  python3 webserv_tester.py http://localhost:8080 http://127.0.0.1:8081")
            print("")
            print("Default servers:")
            print("  Server1: http://localhost:8080")
            print("  Server2: http://127.0.0.1:8081")
            return
    
    # Configure servers based on arguments
    servers = []
    if len(sys.argv) >= 2:
        servers.append({"name": "Server1", "url": sys.argv[1]})
    else:
        servers.append({"name": "Server1", "url": "http://localhost:8080"})
    
    if len(sys.argv) >= 3:
        servers.append({"name": "Server2", "url": sys.argv[2]})
    else:
        servers.append({"name": "Server2", "url": "http://127.0.0.1:8081"})
    
    # Create and run tester
    tester = WebServTester(servers)
    
    try:
        tester.run_all_tests()
    except KeyboardInterrupt:
        print("\n\n⚠️  Testing interrupted by user")
        tester.print_final_summary(0)
    except Exception as e:
        print(f"\n\n💥 Testing failed with error: {str(e)}")
        import traceback
        traceback.print_exc()
    
    # Save results
    try:
        tester.save_results()
        tester.save_curl_commands()
        tester.save_summary_report()
    except Exception as e:
        print(f"⚠️  Could not save results: {e}")

if __name__ == "__main__":
    main()