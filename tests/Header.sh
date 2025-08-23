#!/usr/bin/env bash

HOST="127.0.0.1:8080"
NC="nc -w 2"

# Colors
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
RESET="\033[0m"
BOLD="\033[1m"

# Counters
PASS_COUNT=0
FAIL_COUNT=0

# ========== REQUESTS ==========

NO_HOST="GET / HTTP/1.1\r\n\r\n"
DUP_HOST="GET / HTTP/1.1\r\nHost: $HOST\r\nHost: evil.com\r\n\r\n"
NO_COLON="GET / HTTP/1.1\r\nHost $HOST\r\n\r\n"
SPACE_COLON="GET / HTTP/1.1\r\nHost : $HOST\r\n\r\n"
CL_TE_CONFLICT="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\nHello"
INVALID_TE="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nTransfer-Encoding: bogus\r\n\r\n5\r\nHello\r\n0\r\n\r\n"
MULTI_CL="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nHello"
BIG_HEADER="GET / HTTP/1.1\r\nHost: $HOST\r\nX-Header: $(head -c 9000 < /dev/zero | tr '\0' 'A')\r\n\r\n"
NO_BODY_LEN="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\n\r\nHello"
EXPECT_FAIL="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nExpect: 100discontinue\r\nContent-Length: 5\r\n\r\nHello"
CL_BAD="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nContent-Length: abc\r\n\r\nHello"
CL_SHORT="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nContent-Length: 10\r\n\r\nHi"
CL_LONG="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nContent-Length: 2\r\n\r\nHi!"
PAYLOAD_TOO_LARGE="POST /cgi-bin/ HTTP/1.1\r\nHost: $HOST\r\nContent-Length: 20000000\r\n\r\n$(head -c 20000000 < /dev/zero | tr '\0' 'A')"
URI_TOO_LONG="GET /$(head -c 9000 < /dev/zero | tr '\0' 'A') HTTP/1.1\r\nHost: $HOST\r\n\r\n"
HEADER_TOO_LARGE="GET / HTTP/1.1\r\nHost: $HOST\r\n$(for i in $(seq 1 1000); do echo -n "X-$i: test\r\n"; done)\r\n\r\n"
BAD_VERSION="GET / HTTP/1.2\r\nHost: $HOST\r\n\r\n"
METHOD_NOT_ALLOWED="POST / HTTP/1.1\r\nHost: $HOST\r\n\r\n"
NOT_IMPLEMENTED="BREW / HTTP/1.1\r\nHost: $HOST\r\n\r\n"


# ========== FUNCTIONS ==========

send_request() {
    printf "%b" "$1" | $NC ${HOST%:*} ${HOST#*:}
}

get_status() {
    echo "$1" | head -n 1 | awk '{print $2}'
}

run_test() {
    local name="$1"
    local expected="$2"
    local request="$3"

    local response
    response=$(send_request "$request")
    local actual
    actual=$(get_status "$response")

    if [[ "$expected" == "$actual" ]]; then
        echo -e "${GREEN}[PASS - $actual]${RESET} $name"
        ((PASS_COUNT++))
    elif [[ -z "$actual" ]]; then
        echo -e "${RED}[FAIL - No Response]${RESET} $name"
        ((FAIL_COUNT++))
    else
        echo -e "${RED}[FAIL - Expected $expected, got $actual]${RESET} $name"
        ((FAIL_COUNT++))
    fi
}

# ========== RUN TESTS ==========

echo -e "${BOLD}========== INVALID HEADER TESTS ==========${RESET}"

run_test "No Host header"                  "400" "$NO_HOST"
run_test "Duplicate Host headers"          "400" "$DUP_HOST"
run_test "Invalid header (no colon)"       "400" "$NO_COLON"
run_test "Space before colon"              "400" "$SPACE_COLON"
run_test "Content-Length + TE conflict"    "400" "$CL_TE_CONFLICT"
run_test "Invalid Transfer-Encoding"       "400" "$INVALID_TE"
run_test "Multiple Content-Length headers" "400" "$MULTI_CL"
run_test "Oversized header (>8KB)"         "400" "$BIG_HEADER"
run_test "411 Length Required"             "411" "$NO_BODY_LEN"
run_test "417 Expectation Failed"          "417" "$EXPECT_FAIL"
run_test "Bad Content-Length (non-numeric)" "400" "$CL_BAD"
run_test "Content-Length mismatch (short)" "400" "$CL_SHORT"
run_test "Content-Length mismatch (long)"  "400" "$CL_LONG"
run_test "Payload Too Large (413)"         "413" "$PAYLOAD_TOO_LARGE"
run_test "URI Too Long (414)"              "414" "$URI_TOO_LONG"
run_test "Header Too Large (400)"          "400" "$HEADER_TOO_LARGE"
run_test "HTTP Version Not Supported (505)" "505" "$BAD_VERSION"
run_test "Method Not Allowed (405)"        "405" "$METHOD_NOT_ALLOWED"
run_test "Not Implemented (501)"           "501" "$NOT_IMPLEMENTED"

# ========== SUMMARY ==========

echo -e "\n${BOLD}========== SUMMARY ==========${RESET}"
echo -e "${GREEN}Passed: $PASS_COUNT${RESET}"
echo -e "${RED}Failed: $FAIL_COUNT${RESET}"
