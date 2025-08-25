import os
import sys
import re
from html import escape
import cgi
from datetime import datetime

# Default values
deleted = False
filename = ''
error_message = ''
exit_code = '200'

def log_error(message):
	try:
		with open('script.log', 'a') as log_file:
			timestamp = datetime.now().isoformat()
			log_file.write(f"{timestamp} {message}\n")
	except:
		pass  # Ignore logging errors

# Get environment variables
upload_dir = os.environ.get('UPLOAD_DIR', './uploads/')
request_method = os.environ.get('REQUEST_METHOD', 'GET')

# Ensure upload directory exists
if not os.path.exists(upload_dir):
	os.makedirs(upload_dir, 0o755)

# Handle file deletion
if request_method == 'POST':
	try:
		# Create FieldStorage instance to parse form data
		form = cgi.FieldStorage()
		
		if 'filename' in form:
			filename = form['filename'].value
			
			if filename:
				file_path = os.path.join(upload_dir, filename)

				# Verify file exists and is writable before deletion
				if os.path.exists(file_path) and os.access(file_path, os.W_OK):
					try:
						os.unlink(file_path)
						deleted = True
						exit_code = '200'
						
					except Exception as e:
						error_message = 'Delete operation failed (server error)'
						exit_code = '502'
				else:
					error_message = 'File not found or not writable'
					exit_code = '404'
			else:
				error_message = 'Invalid filename after sanitization'
				exit_code = '400'
		else:
			error_message = 'No filename field found'
			exit_code = '400'
			
	except Exception as e:
		error_message = f'Delete processing error: {str(e)}'
		exit_code = '502'
		
else:
	error_message = 'Invalid request method'
	exit_code = '405'

if error_message:
	log_error(error_message)

# Generate HTML content
html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1" />
	<title>{"File Deleted" if deleted else "Delete Error"}</title>
	<link rel="stylesheet" href="/styles.css" />
	<link rel="icon" type="image/x-icon" href="/favicon.png">
</head>
<body>
	<div class="floating-elements">
		<div class="floating-element"></div>
		<div class="floating-element"></div>
		<div class="floating-element"></div>
	</div>

	<div class="container">"""

if deleted:
	html_content += f"""
		<div style="font-size: 3rem; margin-bottom: 1rem;">🗑️</div>
		<h1 class="title">File "{escape(filename)}" successfully deleted!</h1>
		<p class="subtitle">The file has been removed from the server</p>"""
else:
	html_content += f"""
		<div style="font-size: 3rem; margin-bottom: 1rem;">❌</div>
		<h1 class="title">Deletion Failed</h1>
		<p class="subtitle">{escape(error_message)}</p>"""

html_content += """
		<div style="margin-top: 2rem;">
			<form method="POST" action="upload.py" style="display: inline;">
				<button type="submit" name="back" value="1" class="button">← Back to Upload</button>
			</form>
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