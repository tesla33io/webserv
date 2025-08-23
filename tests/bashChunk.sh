#!/bin/bash

HOST=127.0.0.1
PORT=8080

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # no color

send_request() {
  local name="$1"
  local request="$2"

  echo -e "${YELLOW}>>> Test: $name${NC}"
  response=$(echo -e "$request" | nc -q 1 $HOST $PORT)

  echo -e "Response:\n$response\n"

  if echo "$response" | grep -q "200 OK"; then
    echo -e "${GREEN}[PASS]${NC} $name\n"
  elif echo "$response" | grep -q "400"; then
    echo -e "${RED}[FAIL: 400 Bad Request]${NC} $name\n"
  elif echo "$response" | grep -q "411"; then
    echo -e "${RED}[FAIL: 411 Length Required]${NC} $name\n"
  elif echo "$response" | grep -q "500"; then
    echo -e "${RED}[FAIL: 500 Server Error]${NC} $name\n"
  else
    echo -e "${RED}[UNKNOWN]${NC} $name\n"
  fi
}

# Requests ---------------------------------------------------------

VALID_CHUNKED="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
4\r
Wiki\r
5\r
pedia\r
E\r
 in chunks.   \r
0\r
\r
"

INVALID_CHUNKED="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
Z\r
InvalidSize\r
0\r
\r
"

INCOMPLETE_CHUNKED="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
5\r
Hello\r
"

EMPTY_BODY="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
0\r
\r
"

BIG_CHUNK="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
A\r
0123456789\r
0\r
\r
"

# Run tests --------------------------------------------------------

send_request "Valid chunked" "$VALID_CHUNKED"
send_request "Invalid hex size" "$INVALID_CHUNKED"
send_request "Incomplete (missing final 0)" "$INCOMPLETE_CHUNKED"
send_request "Empty body (just 0 chunk)" "$EMPTY_BODY"
send_request "Big chunk (10 bytes)" "$BIG_CHUNK"
