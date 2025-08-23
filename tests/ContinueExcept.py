import socket

host = "127.0.0.1"
port = 8080

headers = (
    "POST /cgi-bin/ HTTP/1.1\r\n"
    "Host: {0}:{1}\r\n"
    "Content-Length: 11\r\n"
    "Expect: 100-continue\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
).format(host, port)

payload = "Hello World"

with socket.create_connection((host, port)) as s:
    print("[*] Sending headers...")
    s.sendall(headers.encode())

    # Wait for server's intermediate response (100 Continue)
    response = s.recv(4096).decode()
    print("[*] Server response to headers:\n", response)

    if "100 Continue" in response:
        print("[*] Sending payload...")
        s.sendall(payload.encode())

        # Read final server response
        final_response = s.recv(4096).decode()
        print("[*] Final server response:\n", final_response)
    else:
        print("[!] Server did not send 100 Continue")