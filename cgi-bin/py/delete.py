import os
import sys
import re
from html import escape
import cgi
from datetime import datetime
from urllib.parse import parse_qs

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
query_string = os.environ.get('QUERY_STRING', '')

# Ensure upload directory exists
if not os.path.exists(upload_dir):
	os.makedirs(upload_dir, 0o755)

# Handle file deletion
if request_method == 'DELETE':
	try:
		# First, try to get filename from query string
		if query_string:
			query_params = parse_qs(query_string)
			if 'filename' in query_params:
				filename = query_params['filename'][0]
		
		# If not in query string, try reading from stdin (request body)
		if not filename:
			try:
				content_length = int(os.environ.get('CONTENT_LENGTH', 0))
				if content_length > 0:
					input_data = sys.stdin.buffer.read(content_length).decode('utf-8')
					# Try parsing as query string first
					if '=' in input_data:
						parsed_data = parse_qs(input_data)
						if 'filename' in parsed_data:
							filename = parsed_data['filename'][0]
					else:
						# Treat entire body as filename
						filename = input_data.strip()
			except (ValueError, UnicodeDecodeError):
				pass
		
		if filename:
			# Security: prevent directory traversal
			filename = os.path.basename(filename)
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
				if not os.path.exists(file_path):
					error_message = 'File not found'
					exit_code = '404'
				else:
					error_message = 'File not writable'
					exit_code = '403'
		else:
			error_message = 'No filename provided'
			exit_code = '400'
			
	except Exception as e:
		error_message = f'Delete processing error: {str(e)}'
		exit_code = '502'

elif request_method == 'POST':
	# Fallback for form-based deletion
	try:
		form = cgi.FieldStorage()
		
		if 'filename' in form and ('_method' in form and form['_method'].value == 'DELETE'):
			filename = form['filename'].value
			filename = os.path.basename(filename)  # Security
			
			if filename:
				file_path = os.path.join(upload_dir, filename)

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
				error_message = 'Invalid filename'
				exit_code = '400'
		else:
			error_message = 'Missing filename or _method field'
			exit_code = '400'
			
	except Exception as e:
		error_message = f'Form processing error: {str(e)}'
		exit_code = '502'
		
else:
	error_message = f'Invalid request method: {request_method}. Use DELETE or POST'
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
		<p class="subtitle">{escape(error_message)}</p>
		<p style="font-size: 0.8em; opacity: 0.7;">Method: {escape(request_method)}</p>"""

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