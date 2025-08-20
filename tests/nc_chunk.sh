#!/bin/bash

HOST=127.0.0.1
PORT=8080

send_request() {
  echo -e "$1" | nc -q 1 $HOST $PORT
}

# --- VALID CHUNKED REQUEST ---
echo ">>> Sending VALID chunked request"
send_request "POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
4\r
Wiki\r
5\r
pedia\r
E\r
 in chunks.\r
0\r
\r
"

# --- INVALID CHUNKED REQUEST (bad hex size) ---
echo
echo ">>> Sending INVALID chunked request (bad hex length 'Z')"
send_request "POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
Z\r
InvalidSize\r
0\r
\r
"

# --- INCOMPLETE CHUNKED REQUEST (missing terminating 0) ---
echo
echo ">>> Sending INCOMPLETE chunked request (no final 0)"
send_request "POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
5\r
Hello\r
"
