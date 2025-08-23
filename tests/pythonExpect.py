#!/usr/bin/env python3
import socket
import time
import sys

def test_expect_continue(host="127.0.0.1", port=8080):
    """Test Expect: 100-continue functionality"""
    
    print(f"[*] Testing Expect: 100-continue on {host}:{port}")
    
    # Create socket
    try:
        sock = socket.create_connection((host, port), timeout=10)
        sock.settimeout(5.0)  # 5 second timeout for responses
    except Exception as e:
        print(f"[!] Failed to connect: {e}")
        return False
    
    try:
        # Prepare request headers
        headers = (
            "POST /cgi-bin/ HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            "Content-Length: 11\r\n"
            "Expect: 100-continue\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
        )
        
        payload = "Hello World"
        
        print(f"[*] Sending headers:")
        print(headers.replace('\r\n', '\\r\\n\n'))
        
        # Send headers
        sock.sendall(headers.encode())
        print("[*] Headers sent, waiting for 100 Continue...")
        
        # Wait for 100 Continue response
        try:
            response = sock.recv(1024).decode()
            print(f"[*] Server response:")
            print(repr(response))
            
            if "100 Continue" in response:
                print("[✓] Received 100 Continue! Sending body...")
                
                # Send the body
                sock.sendall(payload.encode())
                print(f"[*] Body sent: {payload}")
                
                # Wait for final response
                try:
                    final_response = sock.recv(4096).decode()
                    print(f"[*] Final server response:")
                    print(repr(final_response))
                    print("[✓] Test completed successfully!")
                    return True
                    
                except socket.timeout:
                    print("[!] Timeout waiting for final response")
                    return False
                    
            else:
                print("[!] Did not receive 100 Continue")
                print(f"[!] Got instead: {repr(response)}")
                return False
                
        except socket.timeout:
            print("[!] Timeout waiting for 100 Continue response")
            return False
            
    except Exception as e:
        print(f"[!] Error during test: {e}")
        return False
        
    finally:
        sock.close()

def test_without_expect_continue(host="127.0.0.1", port=8080):
    """Test normal POST request without Expect: 100-continue for comparison"""
    
    print(f"\n[*] Testing normal POST without Expect: 100-continue")
    
    try:
        sock = socket.create_connection((host, port), timeout=10)
        sock.settimeout(5.0)
    except Exception as e:
        print(f"[!] Failed to connect: {e}")
        return False
    
    try:
        # Prepare complete request
        request = (
            "POST /cgi-bin/test HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            "Content-Length: 11\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Hello World"
        )
        
        print(f"[*] Sending complete request:")
        print(request.replace('\r\n', '\\r\\n\n'))
        
        # Send complete request
        sock.sendall(request.encode())
        print("[*] Request sent, waiting for response...")
        
        # Wait for response
        try:
            response = sock.recv(4096).decode()
            print(f"[*] Server response:")
            print(repr(response))
            print("[✓] Normal POST test completed!")
            return True
            
        except socket.timeout:
            print("[!] Timeout waiting for response")
            return False
            
    except Exception as e:
        print(f"[!] Error during test: {e}")
        return False
        
    finally:
        sock.close()

def test_expect_continue_with_delays(host="127.0.0.1", port=8080):
    """Test with delays to better observe the flow"""
    
    print(f"\n[*] Testing Expect: 100-continue with delays")
    
    try:
        sock = socket.create_connection((host, port), timeout=15)
        sock.settimeout(10.0)  # Longer timeout
    except Exception as e:
        print(f"[!] Failed to connect: {e}")
        return False
    
    try:
        # Send headers line by line with delays
        headers = [
            "POST /cgi-bin/test HTTP/1.1\r\n",
            f"Host: {host}:{port}\r\n",
            "Content-Length: 11\r\n",
            "Expect: 100-continue\r\n",
            "Content-Type: text/plain\r\n",
            "\r\n"
        ]
        
        print("[*] Sending headers line by line:")
        for i, header_line in enumerate(headers):
            print(f"    [{i+1}] {repr(header_line)}")
            sock.sendall(header_line.encode())
            if i < len(headers) - 1:  # Don't delay after the final \r\n
                time.sleep(0.1)  # Small delay between header lines
        
        print("[*] All headers sent, waiting for 100 Continue...")
        
        # Wait for 100 Continue
        start_time = time.time()
        try:
            response = sock.recv(1024).decode()
            elapsed = time.time() - start_time
            print(f"[*] Response received after {elapsed:.2f}s:")
            print(repr(response))
            
            if "100 Continue" in response:
                print("[✓] Received 100 Continue! Waiting 1 second before sending body...")
                time.sleep(1)  # Deliberate delay
                
                sock.sendall("Hello World".encode())
                print("[*] Body sent, waiting for final response...")
                
                final_response = sock.recv(4096).decode()
                print(f"[*] Final response:")
                print(repr(final_response))
                return True
            else:
                print(f"[!] Expected 100 Continue, got: {repr(response)}")
                return False
                
        except socket.timeout:
            elapsed = time.time() - start_time
            print(f"[!] Timeout after {elapsed:.2f}s waiting for 100 Continue")
            return False
            
    except Exception as e:
        print(f"[!] Error: {e}")
        return False
        
    finally:
        sock.close()

def main():
    if len(sys.argv) >= 3:
        host = sys.argv[1]
        port = int(sys.argv[2])
    elif len(sys.argv) == 2:
        host = "127.0.0.1"
        port = int(sys.argv[1])
    else:
        host = "127.0.0.1"
        port = 8080
    
    print("=" * 60)
    print("HTTP Expect: 100-continue Tester")
    print("=" * 60)
    
    # Test 1: Basic Expect: 100-continue
    success1 = test_expect_continue(host, port)
    
    # Test 2: Normal POST for comparison
    success2 = test_without_expect_continue(host, port)
    
    # Test 3: With delays to observe timing
    success3 = test_expect_continue_with_delays(host, port)
    
    print("\n" + "=" * 60)
    print("SUMMARY:")
    print(f"  Expect: 100-continue test: {'✓ PASS' if success1 else '✗ FAIL'}")
    print(f"  Normal POST test:          {'✓ PASS' if success2 else '✗ FAIL'}")
    print(f"  Delayed 100-continue test: {'✓ PASS' if success3 else '✗ FAIL'}")
    print("=" * 60)
    
    if success1:
        print("\n[✓] Your server correctly handles Expect: 100-continue!")
    else:
        print("\n[!] Your server has issues with Expect: 100-continue handling")
        print("    Check server logs for debugging information")

if __name__ == "__main__":
    main()