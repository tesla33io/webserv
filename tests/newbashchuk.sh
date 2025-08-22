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
  
  # Use timeout and longer wait time
  response=$(timeout 10s bash -c "echo -e \"$request\" | nc -w 3 $HOST $PORT")
  exit_code=$?
  
  echo -e "Response:\n$response\n"

  if [ $exit_code -eq 124 ]; then
    echo -e "${RED}[TIMEOUT]${NC} $name - Server took too long to respond\n"
  elif [ $exit_code -ne 0 ]; then
    echo -e "${RED}[CONNECTION ERROR]${NC} $name - nc exited with code $exit_code\n"
  elif echo "$response" | grep -q "200 OK"; then
    echo -e "${GREEN}[PASS]${NC} $name\n"
  elif echo "$response" | grep -q "400"; then
    echo -e "${GREEN}[EXPECTED 400]${NC} $name\n"
  elif echo "$response" | grep -q "408"; then
    echo -e "${GREEN}[EXPECTED 408 TIMEOUT]${NC} $name\n"
  elif echo "$response" | grep -q "413"; then
    echo -e "${GREEN}[EXPECTED 413 TOO LARGE]${NC} $name\n"
  elif [ -z "$response" ]; then
    echo -e "${RED}[NO RESPONSE]${NC} $name - Connection closed without response\n"
  else
    echo -e "${RED}[UNKNOWN RESPONSE]${NC} $name\n"
  fi
}

# Test with simpler valid chunked request first
SIMPLE_VALID="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n5\r\nHello\r\n0\r\n\r\n"

VALID_CHUNKED="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\nE\r\n in chunks.\r\n0\r\n\r\n"

INVALID_CHUNKED="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\nZ\r\nInvalidSize\r\n0\r\n\r\n"

INCOMPLETE_CHUNKED="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n5\r\nHello\r\n"

EMPTY_BODY="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n0\r\n\r\n"

BIG_CHUNK="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\nA\r\n0123456789\r\n0\r\n\r\n"

# Run tests
echo "Testing chunked transfer encoding..."
send_request "Simple valid chunked" "$SIMPLE_VALID"
send_request "Valid chunked (Wikipedia)" "$VALID_CHUNKED"
send_request "Invalid hex size" "$INVALID_CHUNKED"
send_request "Empty body (just 0 chunk)" "$EMPTY_BODY"
send_request "Big chunk (10 bytes)" "$BIG_CHUNK"
send_request "Incomplete (missing final 0)" "$INCOMPLETE_CHUNKED"

echo "Done testing."