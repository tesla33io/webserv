#!/usr/bin/env python3
"""
Web Server Error Testing Script
Tests various error conditions and configuration compliance for the web server.
"""

import requests
import sys
import json
from urllib.parse import urljoin
import time

class ServerTester:
    def __init__(self, base_url="http://localhost:8080"):
        self.base_url = base_url.rstrip('/')
        self.session = requests.Session()
        self.results = []
        self.curl_commands = []
        
    def log_curl_command(self, test_name, method, url, headers=None, data=None, special_note=""):
        """Log equivalent curl command for the test"""
        curl_cmd = f"curl -X {method}"
        
        if headers:
            for key, value in headers.items():
                # Escape quotes in header values
                escaped_value = value.replace('"', '\\"')
                curl_cmd += f' -H "{key}: {escaped_value}"'
        
        if data:
            if isinstance(data, str):
                curl_cmd += f' -d "{data}"'
            elif hasattr(data, '__iter__') and not isinstance(data, (str, bytes)):
                # For chunked data, show as regular data in curl
                try:
                    data_str = ''.join([chunk.decode() if isinstance(chunk, bytes) else chunk for chunk in data])
                    curl_cmd += f' -d "{data_str}"'
                except:
                    curl_cmd += ' -d "[chunked data]"'
            else:
                curl_cmd += f' -d "[{len(data) if hasattr(data, "__len__") else "unknown"} bytes]"'
        
        curl_cmd += f' "{url}"'
        
        if special_note:
            curl_cmd += f'  # {special_note}'
            
        self.curl_commands.append({
            "test": test_name,
            "command": curl_cmd,
            "note": special_note
        })

    def log_test(self, test_name, status, details="", expected="", actual=""):
        """Log test results"""
        result = {
            "test": test_name,
            "status": status,
            "details": details,
            "expected": expected,
            "actual": actual,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        self.results.append(result)
        
        status_symbol = "✓" if status == "PASS" else "✗" if status == "FAIL" else "⚠"
        print(f"{status_symbol} {test_name}: {status}")
        if details:
            print(f"   Details: {details}")
        if expected and actual:
            print(f"   Expected: {expected}")
            print(f"   Actual: {actual}")
        print()

    def test_error_pages(self):
        """Test all configured error pages"""
        print("=== Testing Error Pages ===")
        
        error_tests = [
            (400, "Bad Request", "GET", "/?invalid=\x80\x81"),  # Invalid characters
            (403, "Forbidden", "GET", "/forbidden"),
            (404, "Not Found", "GET", "/nonexistent"),
            (405, "Method Not Allowed", "POST", "/"),  # Only GET allowed for /
            (414, "URI Too Long", "GET", "/" + "A" * 8192),  # Very long URI
        ]
        
        for expected_code, error_name, method, path in error_tests:
            try:
                url = urljoin(self.base_url, path)
                self.log_curl_command(f"Error {expected_code} ({error_name})", method, url)
                response = self.session.request(method, url, timeout=10)
                
                if response.status_code == expected_code:
                    self.log_test(f"Error {expected_code} ({error_name})", "PASS",
                                f"Correctly returned {expected_code} for {method} {path}")
                else:
                    self.log_test(f"Error {expected_code} ({error_name})", "FAIL",
                                f"Wrong status code for {method} {path}",
                                f"{expected_code}", f"{response.status_code}")
                                
            except requests.RequestException as e:
                self.log_test(f"Error {expected_code} ({error_name})", "ERROR",
                            f"Request failed: {str(e)}")

    def test_method_restrictions(self):
        """Test method restrictions based on config"""
        print("=== Testing Method Restrictions ===")
        
        # Root location: only GET allowed
        forbidden_methods = ["POST", "PUT", "DELETE", "PATCH"]
        for method in forbidden_methods:
            try:
                response = self.session.request(method, self.base_url + "/", timeout=10)
                if response.status_code == 405:
                    self.log_test(f"Method restriction {method} on /", "PASS",
                                f"{method} correctly rejected with 405")
                else:
                    self.log_test(f"Method restriction {method} on /", "FAIL",
                                f"{method} should return 405",
                                "405", f"{response.status_code}")
            except requests.RequestException as e:
                self.log_test(f"Method restriction {method} on /", "ERROR",
                            f"Request failed: {str(e)}")

        # CGI location: GET, POST, DELETE allowed
        allowed_cgi_methods = ["GET", "POST", "DELETE"]
        forbidden_cgi_methods = ["PUT", "PATCH"]
        
        for method in forbidden_cgi_methods:
            try:
                response = self.session.request(method, self.base_url + "/cgi-bin/test", timeout=10)
                if response.status_code == 405:
                    self.log_test(f"CGI method restriction {method}", "PASS",
                                f"{method} correctly rejected with 405 on /cgi-bin/")
                else:
                    self.log_test(f"CGI method restriction {method}", "FAIL",
                                f"{method} should return 405 on /cgi-bin/",
                                "405", f"{response.status_code}")
            except requests.RequestException as e:
                self.log_test(f"CGI method restriction {method}", "ERROR",
                            f"Request failed: {str(e)}")

    def test_content_length_mismatch(self):
        """Test content-length vs actual body mismatch"""
        print("=== Testing Content-Length Mismatch ===")
        
        # Test with incorrect content-length header
        test_data = "Hello World"
        headers = {
            'Content-Type': 'text/plain',
            'Content-Length': str(len(test_data) + 10)  # Wrong length
        }
        
        try:
            # Try to send POST with mismatched content-length
            response = self.session.post(self.base_url + "/cgi-bin/test", 
                                       data=test_data, 
                                       headers=headers, 
                                       timeout=10)
            
            if response.status_code == 400:
                self.log_test("Content-Length mismatch", "PASS",
                            "Server correctly rejected mismatched Content-Length with 400")
            else:
                self.log_test("Content-Length mismatch", "FAIL",
                            "Server should reject mismatched Content-Length",
                            "400", f"{response.status_code}")
                            
        except requests.RequestException as e:
            self.log_test("Content-Length mismatch", "ERROR",
                        f"Request failed: {str(e)}")

    def test_body_size_limits(self):
        """Test client_max_body_size limits"""
        print("=== Testing Body Size Limits ===")
        
        # Test global limit (1M)
        large_data = "A" * (1024 * 1024 + 1)  # 1MB + 1 byte
        try:
            url = self.base_url + "/"
            self.log_curl_command("Global body size limit", "POST", url, 
                                {"Content-Type": "text/plain"}, "[1MB+1 data]",
                                "Should trigger 413")
            response = self.session.post(url, data=large_data, timeout=30)
            if response.status_code == 413:
                self.log_test("Global body size limit", "PASS",
                            "Large request correctly rejected with 413")
            else:
                self.log_test("Global body size limit", "FAIL",
                            "Large request should return 413",
                            "413", f"{response.status_code}")
        except requests.RequestException as e:
            self.log_test("Global body size limit", "ERROR",
                        f"Request failed: {str(e)}")

        # Test CGI limit (100k)
        medium_data = "B" * (100 * 1024 + 1)  # 100KB + 1 byte
        try:
            url = self.base_url + "/cgi-bin/test"
            self.log_curl_command("CGI body size limit", "POST", url,
                                {"Content-Type": "text/plain"}, "[100KB+1 data]",
                                "Should trigger 413")
            response = self.session.post(url, data=medium_data, timeout=30)
            if response.status_code == 413:
                self.log_test("CGI body size limit", "PASS",
                            "Medium request correctly rejected with 413")
            else:
                self.log_test("CGI body size limit", "FAIL",
                            "Medium request should return 413",
                            "413", f"{response.status_code}")
        except requests.RequestException as e:
            self.log_test("CGI body size limit", "ERROR",
                        f"Request failed: {str(e)}")

    def test_chunked_encoding(self):
        """Test chunked transfer encoding"""
        print("=== Testing Chunked Transfer Encoding ===")
        
        def chunked_data_generator():
            """Generate chunked data"""
            chunks = ["Hello", " ", "World", "!"]
            for chunk in chunks:
                yield chunk.encode('utf-8')
        
        try:
            # Test chunked POST to CGI endpoint
            headers = {
                'Transfer-Encoding': 'chunked',
                'Content-Type': 'text/plain'
            }
            
            # Note: requests library handles chunked encoding automatically
            # when data is an iterator
            response = self.session.post(self.base_url + "/cgi-bin/test",
                                       data=chunked_data_generator(),
                                       headers={'Content-Type': 'text/plain'},
                                       timeout=10)
            
            if response.status_code in [200, 404]:  # 404 is fine if CGI script doesn't exist
                self.log_test("Chunked encoding support", "PASS",
                            f"Chunked request handled (status: {response.status_code})")
            else:
                self.log_test("Chunked encoding support", "FAIL",
                            f"Chunked request failed with status {response.status_code}")
                            
        except requests.RequestException as e:
            self.log_test("Chunked encoding support", "ERROR",
                        f"Chunked request failed: {str(e)}")

    def test_malformed_headers(self):
        """Test various malformed header scenarios"""
        print("=== Testing Malformed Headers ===")
        
        malformed_tests = [
            ("Invalid header name", {"Bad Header!": "value"}),
            ("Missing colon", {"BadHeader": "value"}),  # This is actually valid
            ("Empty header value", {"X-Empty": ""}),
            ("Very long header", {"X-Long": "A" * 8192}),
            ("Invalid characters", {"X-Test": "value\r\n\r\nInjected"}),
        ]
        
        for test_name, headers in malformed_tests:
            try:
                response = self.session.get(self.base_url + "/", 
                                          headers=headers, 
                                          timeout=10)
                
                if response.status_code == 400:
                    self.log_test(f"Malformed header: {test_name}", "PASS",
                                "Server correctly rejected malformed header with 400")
                elif test_name == "Empty header value" and response.status_code == 200:
                    self.log_test(f"Malformed header: {test_name}", "PASS",
                                "Empty header value is allowed (RFC compliant)")
                else:
                    self.log_test(f"Malformed header: {test_name}", "INFO",
                                f"Header accepted with status {response.status_code}")
                                
            except requests.RequestException as e:
                if "BadRequest" in str(e) or "400" in str(e):
                    self.log_test(f"Malformed header: {test_name}", "PASS",
                                "Request library rejected malformed header")
                else:
                    self.log_test(f"Malformed header: {test_name}", "ERROR",
                                f"Request failed: {str(e)}")

    def test_config_compliance(self):
        """Test configuration compliance"""
        print("=== Testing Configuration Compliance ===")
        
        # Test autoindex on root
        try:
            response = self.session.get(self.base_url + "/", timeout=10)
            if "Index of" in response.text or "Directory listing" in response.text:
                self.log_test("Autoindex on root", "PASS",
                            "Directory listing is enabled")
            elif response.status_code == 200:
                self.log_test("Autoindex on root", "INFO",
                            "Got 200 but no clear directory listing (might have index.html)")
            else:
                self.log_test("Autoindex on root", "FAIL",
                            f"Unexpected response: {response.status_code}")
        except requests.RequestException as e:
            self.log_test("Autoindex on root", "ERROR",
                        f"Request failed: {str(e)}")

        # Test autoindex on /images
        try:
            response = self.session.get(self.base_url + "/images/", timeout=10)
            if "Index of" in response.text or "Directory listing" in response.text:
                self.log_test("Autoindex on /images", "PASS",
                            "Directory listing is enabled on /images")
            elif response.status_code == 404:
                self.log_test("Autoindex on /images", "INFO",
                            "Images directory doesn't exist yet")
            else:
                self.log_test("Autoindex on /images", "FAIL",
                            f"Autoindex not working on /images: {response.status_code}")
        except requests.RequestException as e:
            self.log_test("Autoindex on /images", "ERROR",
                        f"Request failed: {str(e)}")

    def test_upload_functionality(self):
        """Test upload functionality and path configuration"""
        print("=== Testing Upload Functionality ===")
        
        # Test if upload path is accessible
        try:
            response = self.session.get(self.base_url + "/images/", timeout=10)
            self.log_test("Upload path accessibility", "INFO",
                        f"Upload path returned status: {response.status_code}")
        except requests.RequestException as e:
            self.log_test("Upload path accessibility", "ERROR",
                        f"Request failed: {str(e)}")

    def test_cgi_extensions(self):
        """Test CGI extension handling"""
        print("=== Testing CGI Extensions ===")
        
        # Test .py extension
        try:
            response = self.session.get(self.base_url + "/cgi-bin/test.py", timeout=10)
            if response.status_code == 404:
                self.log_test("Python CGI extension", "INFO",
                            "Python CGI endpoint not found (expected if no test.py exists)")
            elif response.status_code == 200:
                self.log_test("Python CGI extension", "PASS",
                            "Python CGI script executed successfully")
            else:
                self.log_test("Python CGI extension", "INFO",
                            f"Python CGI returned status: {response.status_code}")
        except requests.RequestException as e:
            self.log_test("Python CGI extension", "ERROR",
                        f"Request failed: {str(e)}")

        # Test .php extension  
        try:
            response = self.session.get(self.base_url + "/cgi-bin/test.php", timeout=10)
            if response.status_code == 404:
                self.log_test("PHP CGI extension", "INFO",
                            "PHP CGI endpoint not found (expected if no test.php exists)")
            elif response.status_code == 200:
                self.log_test("PHP CGI extension", "PASS",
                            "PHP CGI script executed successfully")
            else:
                self.log_test("PHP CGI extension", "INFO",
                            f"PHP CGI returned status: {response.status_code}")
        except requests.RequestException as e:
            self.log_test("PHP CGI extension", "ERROR",
                        f"Request failed: {str(e)}")

    def test_http_version_errors(self):
        """Test HTTP version related errors"""
        print("=== Testing HTTP Version Errors ===")
        
        # Test 505: HTTP Version Not Supported
        import socket
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(('localhost', 8080))
            sock.send(b"GET / HTTP/2.0\r\n\r\n")  # Unsupported version
            response = sock.recv(1024).decode('utf-8', errors='ignore')
            sock.close()
            
            if "505" in response:
                self.log_test("HTTP Version Not Supported (505)", "PASS",
                            "Server correctly rejected HTTP/2.0 with 505")
            elif "400" in response:
                self.log_test("HTTP Version Not Supported (505)", "INFO",
                            "Server rejected with 400 instead of 505 (acceptable)")
            else:
                self.log_test("HTTP Version Not Supported (505)", "FAIL",
                            "Server should reject unsupported HTTP version")
                            
        except Exception as e:
            self.log_test("HTTP Version Not Supported (505)", "ERROR",
                        f"Raw socket test failed: {str(e)}")

        # Test with completely invalid HTTP version
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(('localhost', 8080))
            sock.send(b"GET / HTTP/INVALID\r\n\r\n")
            response = sock.recv(1024).decode('utf-8', errors='ignore')
            sock.close()
            
            if "400" in response or "505" in response:
                self.log_test("Invalid HTTP version format", "PASS",
                            "Server correctly rejected invalid HTTP version")
            else:
                self.log_test("Invalid HTTP version format", "FAIL",
                            "Server should reject malformed HTTP version")
                            
        except Exception as e:
            self.log_test("Invalid HTTP version format", "ERROR",
                        f"Raw socket test failed: {str(e)}")

    def test_directory_redirection(self):
        """Test directory redirection (folder without trailing slash)"""
        print("=== Testing Directory Redirection ===")
        
        # Test redirection for /images -> /images/
        try:
            response = self.session.get(self.base_url + "/images", 
                                      allow_redirects=False, 
                                      timeout=10)
            
            if response.status_code in [301, 302]:
                location = response.headers.get('Location', '')
                if location.endswith('/images/'):
                    self.log_test("Directory redirection", "PASS",
                                f"Correctly redirected /images to /images/ with {response.status_code}")
                else:
                    self.log_test("Directory redirection", "FAIL",
                                f"Redirect location incorrect: {location}")
            elif response.status_code == 404:
                self.log_test("Directory redirection", "INFO",
                            "Images directory doesn't exist (404)")
            else:
                self.log_test("Directory redirection", "FAIL",
                            f"Expected redirect (301/302), got {response.status_code}")
                            
        except requests.RequestException as e:
            self.log_test("Directory redirection", "ERROR",
                        f"Request failed: {str(e)}")

    def test_header_injection(self):
        """Test header injection attempts"""
        print("=== Testing Header Injection ===")
        
        injection_tests = [
            ("CRLF injection", {"X-Test": "value\r\nX-Injected: malicious"}),
            ("Null byte injection", {"X-Test": "value\x00injected"}),
            ("Line folding", {"X-Test": "value\r\n continued"}),
        ]
        
        for test_name, headers in injection_tests:
            try:
                response = self.session.get(self.base_url + "/", 
                                          headers=headers, 
                                          timeout=10)
                
                if response.status_code == 400:
                    self.log_test(f"Header injection: {test_name}", "PASS",
                                "Server correctly rejected header injection")
                else:
                    self.log_test(f"Header injection: {test_name}", "WARN",
                                f"Header injection not detected (status: {response.status_code})")
                                
            except requests.RequestException as e:
                if "400" in str(e) or "Bad" in str(e):
                    self.log_test(f"Header injection: {test_name}", "PASS",
                                "Request library or server rejected injection")
                else:
                    self.log_test(f"Header injection: {test_name}", "ERROR",
                                f"Request failed: {str(e)}")

    def test_chunked_requests(self):
        """Test comprehensive chunked transfer encoding scenarios"""
        print("=== Testing Chunked Transfer Encoding (Comprehensive) ===")
        
        # Test 1: Basic chunked request
        try:
            data = "This is a test of chunked encoding"
            chunks = [data.encode()]
            url = self.base_url + "/cgi-bin/test"
            self.log_curl_command("Basic chunked request", "POST", url, 
                                {"Transfer-Encoding": "chunked"}, data,
                                "Basic chunked transfer")
            
            response = self.session.post(url, data=iter(chunks), timeout=10)
            self.log_test("Basic chunked request", "INFO",
                        f"Basic chunked request returned status: {response.status_code}")
                        
        except requests.RequestException as e:
            self.log_test("Basic chunked request", "ERROR",
                        f"Basic chunked request failed: {str(e)}")

        # Test 2: Multiple small chunks
        try:
            chunks = ["Hello", " ", "World", " ", "from", " ", "chunks", "!"]
            url = self.base_url + "/cgi-bin/test"
            combined_data = "".join(chunks)
            self.log_curl_command("Multiple small chunks", "POST", url,
                                {"Transfer-Encoding": "chunked"}, combined_data,
                                "Multiple small chunks")
            
            response = self.session.post(url, data=iter(chunk.encode() for chunk in chunks), timeout=10)
            self.log_test("Multiple small chunks", "INFO",
                        f"Multiple chunks returned status: {response.status_code}")
                        
        except requests.RequestException as e:
            self.log_test("Multiple small chunks", "ERROR",
                        f"Multiple chunks failed: {str(e)}")

        # Test 3: Large chunks (but under limit)
        try:
            large_chunk = "A" * (50 * 1024)  # 50KB chunk
            chunks = [large_chunk.encode()]
            url = self.base_url + "/cgi-bin/test"
            self.log_curl_command("Large chunks under limit", "POST", url,
                                {"Transfer-Encoding": "chunked"}, "[50KB data]",
                                "Large chunk under 100KB limit")
            
            response = self.session.post(url, data=iter(chunks), timeout=30)
            self.log_test("Large chunks under limit", "INFO",
                        f"Large chunk (50KB) returned status: {response.status_code}")
                        
        except requests.RequestException as e:
            self.log_test("Large chunks under limit", "ERROR",
                        f"Large chunk request failed: {str(e)}")

        # Test 4: Chunked request exceeding CGI body limit (100KB)
        try:
            large_chunks = ["B" * 1024 for _ in range(101)]  # 101KB total
            url = self.base_url + "/cgi-bin/test"
            self.log_curl_command("Chunked exceeding CGI limit", "POST", url,
                                {"Transfer-Encoding": "chunked"}, "[101KB chunked data]",
                                "Should trigger 413 - exceeds 100KB CGI limit")
            
            response = self.session.post(url, data=iter(chunk.encode() for chunk in large_chunks), timeout=30)
            
            if response.status_code == 413:
                self.log_test("Chunked exceeding CGI limit", "PASS",
                            "Large chunked request correctly rejected with 413")
            else:
                self.log_test("Chunked exceeding CGI limit", "FAIL",
                            f"Expected 413 for chunked request >100KB, got {response.status_code}")
                            
        except requests.RequestException as e:
            self.log_test("Chunked exceeding CGI limit", "ERROR",
                        f"Large chunked request failed: {str(e)}")

        # Test 5: Chunked request exceeding global limit (1MB)
        try:
            huge_chunks = ["C" * (10 * 1024) for _ in range(110)]  # ~1.1MB total
            url = self.base_url + "/"
            self.log_curl_command("Chunked exceeding global limit", "POST", url,
                                {"Transfer-Encoding": "chunked"}, "[1.1MB chunked data]",
                                "Should trigger 413 - exceeds 1MB global limit")
            
            response = self.session.post(url, data=iter(chunk.encode() for chunk in huge_chunks), timeout=30)
            
            if response.status_code == 413:
                self.log_test("Chunked exceeding global limit", "PASS",
                            "Huge chunked request correctly rejected with 413")
            elif response.status_code == 405:
                self.log_test("Chunked exceeding global limit", "INFO",
                            "POST not allowed on / (405) - normal behavior")
            else:
                self.log_test("Chunked exceeding global limit", "WARN",
                            f"Expected 413 or 405, got {response.status_code}")
                            
        except requests.RequestException as e:
            self.log_test("Chunked exceeding global limit", "ERROR",
                        f"Huge chunked request failed: {str(e)}")

        # Test 6: Empty chunks
        try:
            chunks = ["", "Hello", "", "World", ""]
            url = self.base_url + "/cgi-bin/test"
            self.log_curl_command("Chunked with empty chunks", "POST", url,
                                {"Transfer-Encoding": "chunked"}, "HelloWorld",
                                "Contains empty chunks")
            
            response = self.session.post(url, data=iter(chunk.encode() for chunk in chunks), timeout=10)
            self.log_test("Chunked with empty chunks", "INFO",
                        f"Empty chunks handling returned status: {response.status_code}")
                        
        except requests.RequestException as e:
            self.log_test("Chunked with empty chunks", "ERROR",
                        f"Empty chunks request failed: {str(e)}")

        # Test 7: Binary data chunks
        try:
            binary_chunks = [b'\x00\x01\x02\x03', b'\x04\x05\x06\x07', b'\x08\x09\x0a\x0b']
            url = self.base_url + "/cgi-bin/test"
            self.log_curl_command("Binary chunked data", "POST", url,
                                {"Transfer-Encoding": "chunked", "Content-Type": "application/octet-stream"}, 
                                "[binary data]", "Binary chunks")
            
            response = self.session.post(url, data=iter(binary_chunks), 
                                       headers={"Content-Type": "application/octet-stream"}, timeout=10)
            self.log_test("Binary chunked data", "INFO",
                        f"Binary chunks returned status: {response.status_code}")
                        
        except requests.RequestException as e:
            self.log_test("Binary chunked data", "ERROR",
                        f"Binary chunks request failed: {str(e)}")

        # Test 8: Chunked with custom content-type
        try:
            json_data = '{"message": "test", "chunked": true}'
            chunks = [json_data.encode()]
            url = self.base_url + "/cgi-bin/test"
            headers = {"Content-Type": "application/json", "Transfer-Encoding": "chunked"}
            self.log_curl_command("Chunked JSON data", "POST", url, headers, json_data,
                                "JSON with chunked encoding")
            
            response = self.session.post(url, data=iter(chunks), 
                                       headers={"Content-Type": "application/json"}, timeout=10)
            self.log_test("Chunked JSON data", "INFO",
                        f"Chunked JSON returned status: {response.status_code}")
                        
        except requests.RequestException as e:
            self.log_test("Chunked JSON data", "ERROR",
                        f"Chunked JSON request failed: {str(e)}")

        # Test 9: Many small chunks (stress test)
        try:
            many_chunks = [f"chunk{i}".encode() for i in range(100)]
            url = self.base_url + "/cgi-bin/test"
            self.log_curl_command("Many small chunks stress test", "POST", url,
                                {"Transfer-Encoding": "chunked"}, "[100 small chunks]",
                                "Stress test with many chunks")
            
            response = self.session.post(url, data=iter(many_chunks), timeout=30)
            self.log_test("Many small chunks stress test", "INFO",
                        f"100 chunks stress test returned status: {response.status_code}")
                        
        except requests.RequestException as e:
            self.log_test("Many small chunks stress test", "ERROR",
                        f"Many chunks stress test failed: {str(e)}")

        # Test 10: Chunked with additional headers
        try:
            data = "Test with extra headers"
            chunks = [data.encode()]
            url = self.base_url + "/cgi-bin/test"
            headers = {
                "Transfer-Encoding": "chunked",
                "X-Custom-Header": "test-value",
                "User-Agent": "WebServTester/1.0",
                "Accept": "text/plain"
            }
            self.log_curl_command("Chunked with extra headers", "POST", url, headers, data,
                                "Chunked request with additional headers")
            
            response = self.session.post(url, data=iter(chunks), headers={
                "X-Custom-Header": "test-value",
                "User-Agent": "WebServTester/1.0",
                "Accept": "text/plain"
            }, timeout=10)
            self.log_test("Chunked with extra headers", "INFO",
                        f"Chunked with headers returned status: {response.status_code}")
                        
        except requests.RequestException as e:
            self.log_test("Chunked with extra headers", "ERROR",
                        f"Chunked with headers failed: {str(e)}")

    def test_server_responsiveness(self):
        """Test basic server responsiveness"""
        print("=== Testing Server Responsiveness ===")
        
        try:
            start_time = time.time()
            response = self.session.get(self.base_url + "/", timeout=10)
            end_time = time.time()
            
            response_time = end_time - start_time
            
            if response.status_code in [200, 404]:
                self.log_test("Server responsiveness", "PASS",
                            f"Server responding in {response_time:.2f}s")
            else:
                self.log_test("Server responsiveness", "WARN",
                            f"Unexpected status: {response.status_code}")
                            
        except requests.RequestException as e:
            self.log_test("Server responsiveness", "FAIL",
                        f"Server not responding: {str(e)}")

    def run_all_tests(self):
        """Run all tests"""
        print(f"Starting tests against {self.base_url}")
        print("=" * 50)
        
        self.test_server_responsiveness()
        self.test_error_pages()
        self.test_method_restrictions()
        self.test_directory_redirection()
        self.test_content_length_mismatch()
        self.test_body_size_limits()
        self.test_malformed_headers()
        self.test_header_injection()
        self.test_chunked_requests()
        self.test_http_version_errors()
        self.test_config_compliance()
        self.test_upload_functionality()
        self.test_cgi_extensions()
        
        self.print_summary()

    def print_summary(self):
        """Print test summary"""
        print("=" * 50)
        print("TEST SUMMARY")
        print("=" * 50)
        
        pass_count = sum(1 for r in self.results if r["status"] == "PASS")
        fail_count = sum(1 for r in self.results if r["status"] == "FAIL")
        error_count = sum(1 for r in self.results if r["status"] == "ERROR")
        warn_count = sum(1 for r in self.results if r["status"] == "WARN")
        info_count = sum(1 for r in self.results if r["status"] == "INFO")
        
        print(f"✓ PASSED: {pass_count}")
        print(f"✗ FAILED: {fail_count}")
        print(f"⚠ WARNINGS: {warn_count}")
        print(f"ℹ INFO: {info_count}")
        print(f"💥 ERRORS: {error_count}")
        print(f"Total tests: {len(self.results)}")
        
        if fail_count > 0:
            print("\nFAILED TESTS:")
            for result in self.results:
                if result["status"] == "FAIL":
                    print(f"  - {result['test']}: {result['details']}")
        
        if error_count > 0:
            print("\nERRORS:")
            for result in self.results:
                if result["status"] == "ERROR":
                    print(f"  - {result['test']}: {result['details']}")

    def save_results(self, filename="python_test/test_results.json"):
        """Save test results to JSON file"""
        with open(filename, 'w') as f:
            json.dump(self.results, f, indent=2)
        print(f"\nResults saved to {filename}")

    def save_curl_commands(self, filename="python_test/curl_commands.txt"):
        """Save equivalent curl commands to text file"""
        with open(filename, 'w') as f:
            f.write("# Equivalent curl commands for webserver tests\n")
            f.write("# Generated by Web Server Error Testing Script\n")
            f.write(f"# Target server: {self.base_url}\n")
            f.write(f"# Generated at: {time.strftime('%Y-%m-%d %H:%M:%S')}\n\n")
            
            current_category = ""
            for cmd in self.curl_commands:
                # Group commands by test category
                test_category = cmd["test"].split(":")[0] if ":" in cmd["test"] else cmd["test"]
                if test_category != current_category:
                    f.write(f"\n# === {test_category} ===\n")
                    current_category = test_category
                
                f.write(f"# Test: {cmd['test']}\n")
                f.write(f"{cmd['command']}\n")
                if cmd["note"]:
                    f.write(f"# Note: {cmd['note']}\n")
                f.write("\n")
        
        print(f"Curl commands saved to {filename}")


def main():
    """Main function"""
    if len(sys.argv) > 1:
        base_url = sys.argv[1]
    else:
        base_url = "http://localhost:8080"
    
    print("Web Server Error Testing Script")
    print("==============================")
    print(f"Target server: {base_url}")
    print()
    
    tester = ServerTester(base_url)
    tester.run_all_tests()
    
    # Optionally save results
    try:
        tester.save_results()
        tester.save_curl_commands()
    except Exception as e:
        print(f"Could not save results: {e}")


if __name__ == "__main__":
    main()