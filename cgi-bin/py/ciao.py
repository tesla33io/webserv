import os
import sys
import re
from html import escape

# Default values
name = 'Guest'
exit_code = '200'

# Get query parameters from environment variables
query_string = os.environ.get('QUERY_STRING', '')

# Parse query parameters
if query_string:
	params = dict(pair.split('=') for pair in query_string.split('&') if '=' in pair)
	if 'name' in params:
		# Remove leading/trailing whitespace
		name = params['name'].strip()
		
		# Limit to 50 characters
		if len(name) > 50:
			name = name[:50]
		
		# Allow only letters, numbers, spaces, periods, apostrophes, and hyphens
		if not re.match(r'^[\w .\'-]+$', name, re.UNICODE):
			name = 'Guest'
		
		# Escape HTML special characters
		name = escape(name)

# Generate HTML content
html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1" />
	<title>Greeting</title>
	<link rel="stylesheet" href="/styles.css" />
	<link rel="icon" type="image/x-icon" href="/favicon.png">
</head>
<body>
	<div class="floating-elements">
		<div class="floating-element"></div>
		<div class="floating-element"></div>
		<div class="floating-element"></div>
	</div>

	<div class="container">
		<h1 class="title">Hello, {name}!</h1>
		<p class="subtitle">Nice to meet you and welcome to WhateverX :)</p>
		
		<div class="form-row" style="justify-content: center; margin-top: 40px;">
			<a href="/index.html" class="button">Back to Home</a>
		</div>
	</div>
</body>
</html>"""

# Calculate content length
content_length = len(html_content.encode('utf-8'))

# Send exit_code
print(f"{exit_code}\r\n", end="")
print("\r")

# Send content
print(html_content)