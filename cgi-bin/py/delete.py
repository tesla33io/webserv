import cgi
import cgitb
import html
import os
from datetime import datetime
from pathlib import Path

# Enable CGI error reporting
cgitb.enable()

upload_dir = os.environ.get('UPLOAD_DIR', './uploads/')
if not upload_dir.endswith('/'):
	upload_dir += '/'

deleted = False
error_message = ''

# Create FieldStorage instance to parse form data
form = cgi.FieldStorage()

# Only allow POST requests with filename
if os.environ.get('REQUEST_METHOD') == 'POST' and 'filename' in form:
	filename = form['filename'].value
	
	# Security checks
	filename = os.path.basename(filename)  # Prevent directory traversal
	
	if filename:
		file_path = upload_dir + filename
		
		# Verify before deletion
		if os.path.exists(file_path) and os.access(file_path, os.W_OK):
			try:
				os.unlink(file_path)
				deleted = True
			except Exception as e:
				deleted = False
				error_message = 'Delete operation failed (server error)'
		else:
			error_message = 'File not found or not writable'
	else:
		error_message = 'Invalid filename'
else:
	error_message = 'Invalid request method or missing filename'

if error_message:
	try:
		with open('../../script.log', 'a') as stderr:
			stderr.write(f"{datetime.now().isoformat()} {error_message}\n")
	except:
		pass

# Print HTTP headers
print("Content-Type: text/html\n")

# Generate HTML
title = 'Deleted' if deleted else 'Error'

html_content = f'''<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1" />
	<title>{title}</title>
	<link rel="stylesheet" href="/styles.css" />
	<link rel="icon" type="image/x-icon" href="/favicon.png">
</head>
<body>
	<div class="floating-elements">
		<div class="floating-element"></div>
		<div class="floating-element"></div>
		<div class="floating-element"></div>
	</div>

	<div class="container">'''

if deleted:
	html_content += f'''
		<div style="font-size: 3rem; margin-bottom: 1rem;">🗑️</div>
		<h1 class="title">File deleted</h1>
		<p class="subtitle">{html.escape(filename)} was removed</p>'''
else:
	html_content += f'''
		<div style="font-size: 3rem; margin-bottom: 1rem;">❌</div>
		<h1 class="title">Deletion Failed</h1>
		<p class="subtitle">{html.escape(error_message)}</p>'''

html_content += '''
		<div style="margin-top: 2rem;">
			<form method="POST" action="upload.py" style="display: inline;">
				<button type="submit" name="back" value="1" class="button">← Back to Upload</button>
			</form>
		</div>
	</div>
</body>
</html>'''

print(html_content)