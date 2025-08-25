#!/bin/bash

HOST=127.0.0.1
PORT=8080

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # no color

send_request() {
  local name="$1"
  local request="$2"
  local timeout="${3:-3}"  # Default 3 second timeout
  local expect_response="${4:-true}"  # Whether we expect a response

  echo -e "${NC}>>> Test: $name${NC}"
  
  if [ "$expect_response" = "false" ]; then
    echo -e "${BLUE}(Expecting timeout/no response)${NC}"
    response=$(timeout $timeout bash -c "echo -e '$request' | nc $HOST $PORT" 2>/dev/null)
    if [ $? -eq 124 ]; then
      echo -e "${GREEN}[PASS - TIMEOUT AS EXPECTED]${NC} $name\n"
      return
    fi
  else
    response=$(timeout $timeout bash -c "echo -e '$request' | nc $HOST $PORT" 2>/dev/null)
  fi

  # Print only first 5 lines
  echo -e "$(echo "$response" | head -n 1)"


  if echo "$response" | grep -q "200 OK"; then
    echo -e "${GREEN}[PASS]${NC} $name\n"
  elif echo "$response" | grep -q "400"; then
    echo -e "${GREEN}[PASS - 400 Bad Request]${NC} $name\n"
  elif echo "$response" | grep -q "411"; then
    echo -e "${RED}[FAIL: 411 Length Required]${NC} $name\n"
  elif echo "$response" | grep -q "413"; then
    echo -e "${GREEN}[PASS - 413 Request Entity Too Large]${NC} $name\n"
  elif echo "$response" | grep -q "500"; then
    echo -e "${RED}[FAIL: 500 Server Error]${NC} $name\n"
  elif [ -z "$response" ]; then
    echo -e "${RED}[FAIL: NO RESPONSE]${NC} $name\n"
  else
    echo -e "${RED}[UNKNOWN RESPONSE]${NC} $name\n"
  fi
}

echo -e "${BLUE}========== CHUNKED REQUEST TESTS ==========${NC}\n"

# ============== VALID TESTS ==============

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
 in chunks.000\r
0\r
\r
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

SINGLE_BYTE_CHUNKS="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
1\r
A\r
1\r
B\r
1\r
C\r
0\r
\r
"

# ============== ERROR TESTS ==============

EXCEED_BODY="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
5\r
Hello\r
5\r
World\r
6\r
!!!!!!\r
6\r
123456\r
5\r
ABCDE\r
5\r
FGHIJ\r
5\r
KLMNO\r
5\r
PQRST\r
5\r
UVWXY\r
5\r
Zabcd\r
0\r
\r
"

# Invalid hex size
INVALID_HEX="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
Z\r
InvalidSize\r
0\r
\r
"

# Missing CRLF after chunk data
MISSING_CRLF_DATA="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
5\r
HelloWorld\r
0\r
\r
"

# Chunk size mismatch (size says 5, data is 3 chars)
SIZE_MISMATCH_SMALL="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
5\r
Hi\r
0\r
\r
"

# Chunk size mismatch (size says 2, data is 5 chars)
SIZE_MISMATCH_BIG="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
2\r
Hello\r
0\r
\r
"

# Empty chunk size line
EMPTY_CHUNK_SIZE="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
\r
Hello\r
0\r
\r
"

# Negative chunk size
NEGATIVE_CHUNK="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
-5\r
Hello\r
0\r
\r
"


# Missing terminating chunk entirely
NO_TERMINATING_CHUNK="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
5\r
Hello\r
5\r
World\r"

# Incomplete chunk (missing data and terminator)
INCOMPLETE_CHUNK="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
5\r
Hello\r"

# Chunk with only size, no data or CRLF
ONLY_SIZE="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
5"

# ============== STRESS TESTS ==============

# 1KB chunk
LARGE_DATA=$(printf 'X%.0s' {1..1024})
LARGE_CHUNK_1KB="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
$(printf '%X' ${#LARGE_DATA})\r
$LARGE_DATA\r
0\r
\r
"

# 100KB data
LARGE_DATA2=$(head -c 102400 < /dev/zero | tr '\0' 'X')
LARGE_CHUNK_100KB="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r
$(printf '%X' ${#LARGE_DATA2})\r
$LARGE_DATA2\r
0\r
\r
"


# Many small chunks (10,000 x 1 byte)
MANY_CHUNKS="POST /cgi-bin/ HTTP/1.1\r
Host: $HOST\r
Transfer-Encoding: chunked\r
Content-Type: text/plain\r
\r"
for i in $(seq 1 10000); do
    MANY_CHUNKS+="1\r
$((i % 10))\r"
done
MANY_CHUNKS+="0\r
\r"




# ============== RUN TESTS ==============

echo -e "${YELLOW}=== VALID TESTS (Should return 200 OK) ===${NC}"
send_request "Valid chunked request" "$VALID_CHUNKED"
send_request "Empty body (0 chunk only)" "$EMPTY_BODY"
send_request "Big chunk (10 bytes)" "$BIG_CHUNK"
send_request "Single byte chunks" "$SINGLE_BYTE_CHUNKS"

echo -e "${YELLOW}=== ERROR TESTS (Should return 400 Bad Request) ===${NC}"
send_request "Invalid hex size (Z)" "$INVALID_HEX"
send_request "Missing CRLF after chunk data" "$MISSING_CRLF_DATA"
send_request "Chunk size too big for data" "$SIZE_MISMATCH_SMALL"
send_request "Chunk size too small for data" "$SIZE_MISMATCH_BIG"
send_request "Empty chunk size line" "$EMPTY_CHUNK_SIZE"
send_request "Negative chunk size" "$NEGATIVE_CHUNK"
send_request "Exceed body" "$EXCEED_BODY"


echo -e "${YELLOW}=== INCOMPLETE TESTS (May timeout - server waiting for more data) ===${NC}"
send_request "No terminating chunk" "$NO_TERMINATING_CHUNK" 5 false
send_request "Incomplete chunk (missing data)" "$INCOMPLETE_CHUNK" 5 false
send_request "Only chunk size, no data" "$ONLY_SIZE" 5 false

echo -e "${YELLOW}=== STRESS TESTS ===${NC}"
send_request "Large chunk (1KB)" "$LARGE_CHUNK_1KB"
send_request "Large chunk (100KB - unreliable)" "$LARGE_CHUNK_1MB"
send_request "Many small chunks (10,000x)" "$MANY_CHUNKS"

echo -e "\n${GREEN}========== TESTS COMPLETED ==========${NC}"