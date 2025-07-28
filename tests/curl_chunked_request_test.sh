#!/bin/bash

DATA="This is first chunk
This one will be the second one
The size of this chunk is 1e
Last chunk is this one"

echo "$DATA" | curl -v -T. -X GET -H "Transfer-Encoding: chunked" http://127.0.0.1:8080/

