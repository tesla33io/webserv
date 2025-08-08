import cgitb
import html
import re
import urllib.parse
import os

# Enable CGI error reporting
cgitb.enable()

# Default name if none provided
name = 'Guest'

# Get query parameters
query_string = os.environ.get('QUERY_STRING', '')
params = urllib.parse.parse_qs(query_string)

# Check if 'name' is set in the URL query parameters
if 'name' in params:
	# Remove leading/trailing whitespace
	name = params['name'][0].strip()
	
	# Limit the name to 50 characters to avoid abuse or layout issues
	if len(name) > 50:
		name = name[:50]
	
	# Allow only letters, numbers, spaces, periods, apostrophes, and hyphens
	# Using \w for Unicode letters/numbers plus space, ., ', -
	# Excluding underscore manually
	if not re.match(r'^[\w .\'\-]+$', name, re.UNICODE) or '_' in name:
		name = 'Guest'  # Reset to default if input is invalid
	
	# Convert special HTML characters to safe entities (e.g., < becomes &lt;)
	# Prevents Cross-Site Scripting (XSS)
	name = html.escape(name)

# Print HTTP headers
print("Content-Type: text/html\n")

# Print HTML content
print(f"""<!DOCTYPE html>
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
</html>""")
