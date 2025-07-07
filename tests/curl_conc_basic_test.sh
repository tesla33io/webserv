#!/bin/bash

if [[ $# -lt 2 ]]; then
    echo "Not enough arguments" 1>&2
    echo "Usage: $0 <TARGET> <NUM_CLIENTS> [<REQUEST_METHOD> <CONTENT_LENGTH>]" 1>&2
    exit 1
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
SERVER_HOST="127.0.0.1"
SERVER_PORT="8080"
TARGET="$1"
ENDPOINT="http://${SERVER_HOST}:${SERVER_PORT}/${TARGET}"
NUM_REQUESTS=$2
TIMEOUT=5
REQUEST_METHOD=${4:-GET} # studpid me
CONTENT_LENGTH=${3:-0}

echo $CONTENT_LENGTH
echo $REQUEST_METHOD

echo -e "${YELLOW}Testing concurrent connections to: $ENDPOINT${NC}"
echo -e "${YELLOW}Number of parallel requests: $NUM_REQUESTS${NC}"
echo ""

# Check if server is running
echo "Checking if server is accessible..."
if ! timeout 2 curl -s "$ENDPOINT" > /dev/null 2>&1; then
    echo -e "${RED}ERROR: Server is not accessible at $ENDPOINT${NC}"
    echo "Make sure your server is running on port $SERVER_PORT"
    exit 1
fi
echo -e "${GREEN}Server is accessible${NC}"
echo ""

# Create temporary directory
TEMP_DIR=$(mktemp -d)
echo "Using temporary directory: $TEMP_DIR"

# Cleanup function
cleanup() {
    # rm -fr ./test_results_*
    echo "Copying results to ./test_results_$(date +%Y%m%d_%H%M%S)"
    cp -r "$TEMP_DIR" "./test_results_$(date +%Y%m%d_%H%M%S)"
    rm -fr "$TEMP_DIR"
}
trap cleanup EXIT

# Function to send a single request
send_request() {
    local id=$1
    local start_time=$(date +%s.%N)
    local body=""

    if [[ $CONTENT_LENGTH -gt 0 ]]; then
        body=$(cat /dev/urandom | base64 | head -c $CONTENT_LENGTH)
    fi

    # Send request with detailed timing and error info
    local response
    if [[ $CONTENT_LENGTH -gt 0 ]]; then
        response=$(timeout $TIMEOUT curl -s -w "HTTPCODE:%{http_code}|TIME:%{time_total}|SIZE:%{size_download}" \
            -X "$REQUEST_METHOD" -d "$body" "$ENDPOINT" 2>&1)
    else
        response=$(timeout $TIMEOUT curl -s -w "HTTPCODE:%{http_code}|TIME:%{time_total}|SIZE:%{size_download}" \
            -X "$REQUEST_METHOD" "$ENDPOINT" 2>&1)
    fi

    local curl_exit_code=$?
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "N/A")

    # Parse curl output
    if [[ $response == *"HTTPCODE:"* ]]; then
        local body_content=$(echo "$response" | sed 's/HTTPCODE:.*$//')
        local http_code=$(echo "$response" | grep -o 'HTTPCODE:[0-9]*' | cut -d: -f2)
        local curl_time=$(echo "$response" | grep -o 'TIME:[0-9.]*' | cut -d: -f2)
        local size=$(echo "$response" | grep -o 'SIZE:[0-9]*' | cut -d: -f2)
    else
        local body_content="$response"
        local http_code="000"
        local curl_time="N/A"
        local size="0"
    fi

    # Save response body
    echo "$body_content" > "$TEMP_DIR/response_$id"

    # Save metadata
    echo "Request ID: $id" > "$TEMP_DIR/meta_$id"
    echo "HTTP Code: $http_code" >> "$TEMP_DIR/meta_$id"
    echo "Curl Exit Code: $curl_exit_code" >> "$TEMP_DIR/meta_$id"
    echo "Total Duration: ${duration}s" >> "$TEMP_DIR/meta_$id"
    echo "Curl Time: ${curl_time}s" >> "$TEMP_DIR/meta_$id"
    echo "Response Size: $size bytes" >> "$TEMP_DIR/meta_$id"
    echo "Start Time: $start_time" >> "$TEMP_DIR/meta_$id"
    echo "End Time: $end_time" >> "$TEMP_DIR/meta_$id"

    # Print progress
    if [ $curl_exit_code -eq 0 ] && [ "$http_code" = "200" ]; then
        echo -e "${GREEN}✓${NC} Request $id: HTTP $http_code (${duration}s)"
    elif [ $curl_exit_code -eq 124 ]; then
        echo -e "${RED}✗${NC} Request $id: TIMEOUT after ${TIMEOUT}s"
    else
        echo -e "${RED}✗${NC} Request $id: HTTP $http_code, Exit Code $curl_exit_code"
    fi
}

# Export function so it can be used with parallel execution
export -f send_request
export TEMP_DIR ENDPOINT TIMEOUT RED GREEN NC CONTENT_LENGTH REQUEST_METHOD

echo "Starting concurrent requests..."
start_time=$(date +%s.%N)

# Send requests in parallel using background processes
pids=()
for i in $(seq 1 $NUM_REQUESTS); do
    send_request $i &
    pids+=($!)
done

# Wait for all requests to complete
echo "Waiting for all requests to complete..."
wait_failed=false
for pid in "${pids[@]}"; do
    if ! wait $pid; then
        wait_failed=true
    fi
done

end_time=$(date +%s.%N)
total_duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "N/A")

echo ""
echo "All requests completed in ${total_duration}s"
echo ""

# Analyze results
echo "=== ANALYSIS ==="

# Count successful requests
successful=0
failed=0
timeouts=0

for i in $(seq 1 $NUM_REQUESTS); do
    if [ -f "$TEMP_DIR/meta_$i" ]; then
        http_code=$(grep "HTTP Code:" "$TEMP_DIR/meta_$i" | cut -d: -f2 | tr -d ' ')
        curl_exit=$(grep "Curl Exit Code:" "$TEMP_DIR/meta_$i" | cut -d: -f2 | tr -d ' ')
        
        if [ "$curl_exit" = "124" ]; then
            ((timeouts++))
        elif [ "$curl_exit" = "0" ] && [ "$http_code" = "200" ]; then
            ((successful++))
        else
            ((failed++))
        fi
    else
        ((failed++))
    fi
done

echo "Successful requests: $successful/$NUM_REQUESTS"
echo "Failed requests: $failed/$NUM_REQUESTS"
echo "Timed out requests: $timeouts/$NUM_REQUESTS"

# Check response consistency
if [ $successful -gt 1 ]; then
    echo ""
    echo "Checking response consistency..."
    
    # Find first successful response as baseline
    baseline=""
    for i in $(seq 1 $NUM_REQUESTS); do
        if [ -f "$TEMP_DIR/meta_$i" ]; then
            http_code=$(grep "HTTP Code:" "$TEMP_DIR/meta_$i" | cut -d: -f2 | tr -d ' ')
            curl_exit=$(grep "Curl Exit Code:" "$TEMP_DIR/meta_$i" | cut -d: -f2 | tr -d ' ')
            if [ "$curl_exit" = "0" ] && [ "$http_code" = "200" ]; then
                baseline="$TEMP_DIR/response_$i"
                break
            fi
        fi
    done
    
    if [ -n "$baseline" ]; then
        identical=true
        for i in $(seq 1 $NUM_REQUESTS); do
            if [ -f "$TEMP_DIR/meta_$i" ]; then
                http_code=$(grep "HTTP Code:" "$TEMP_DIR/meta_$i" | cut -d: -f2 | tr -d ' ')
                curl_exit=$(grep "Curl Exit Code:" "$TEMP_DIR/meta_$i" | cut -d: -f2 | tr -d ' ')
                if [ "$curl_exit" = "0" ] && [ "$http_code" = "200" ]; then
                    if ! diff -q "$baseline" "$TEMP_DIR/response_$i" > /dev/null 2>&1; then
                        echo -e "${RED}Response $i differs from baseline${NC}"
                        identical=false
                    fi
                fi
            fi
        done
        
        if $identical; then
            echo -e "${GREEN}✓ All successful responses are identical${NC}"
        else
            echo -e "${RED}✗ Responses differ - possible concurrency issue${NC}"
        fi
    fi
fi

# Performance summary
if [ $successful -gt 0 ]; then
    echo ""
    echo "Performance Summary:"
    echo "- Average concurrency: $(echo "scale=2; $NUM_REQUESTS / $total_duration" | bc -l 2>/dev/null || echo "N/A") req/s"
    
    # Calculate average response time from successful requests
    total_time=0
    count=0
    for i in $(seq 1 $NUM_REQUESTS); do
        if [ -f "$TEMP_DIR/meta_$i" ]; then
            http_code=$(grep "HTTP Code:" "$TEMP_DIR/meta_$i" | cut -d: -f2 | tr -d ' ')
            curl_exit=$(grep "Curl Exit Code:" "$TEMP_DIR/meta_$i" | cut -d: -f2 | tr -d ' ')
            if [ "$curl_exit" = "0" ] && [ "$http_code" = "200" ]; then
                duration=$(grep "Total Duration:" "$TEMP_DIR/meta_$i" | cut -d: -f2 | tr -d 's ' | head -1)
                if [[ $duration =~ ^[0-9]+\.?[0-9]*$ ]]; then
                    total_time=$(echo "$total_time + $duration" | bc -l 2>/dev/null || echo "$total_time")
                    ((count++))
                fi
            fi
        fi
    done
    
    if [ $count -gt 0 ]; then
        avg_time=$(echo "scale=3; $total_time / $count" | bc -l 2>/dev/null || echo "N/A")
        echo "- Average response time: ${avg_time}s"
    fi
fi

echo ""
if [ $successful -eq $NUM_REQUESTS ]; then
    echo -e "${GREEN}✓ PASS: All requests completed successfully${NC}"
    exit 0
elif [ $successful -gt 0 ]; then
    echo -e "${YELLOW}⚠ PARTIAL: $successful/$NUM_REQUESTS requests succeeded${NC}"
    exit 1
else
    echo -e "${RED}✗ FAIL: No requests succeeded${NC}"
    exit 2
fi
