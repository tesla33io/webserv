import os
import sys
import html
import re
import cgi
import cgitb
from datetime import datetime
import tempfile
import shutil

# Get environment variables
upload_dir = os.environ.get('UPLOAD_DIR', './uploads/')
request_method = os.environ.get('REQUEST_METHOD', 'GET')
content_length = int(os.environ.get('CONTENT_LENGTH', '0'))

# Ensure upload directory exists
if not os.path.exists(upload_dir):
	os.makedirs(upload_dir, 0o755)

uploaded = False
filename = ''
filesize = 0
file_extension = ''
error_message = ''
exit_code = '200'

def get_file_icon(extension):
	icons = {
		'pdf': '📄', 'doc': '📝', 'docx': '📝', 'txt': '📄',
		'jpg': '🖼️', 'jpeg': '🖼️', 'png': '🖼️', 'gif': '🖼️', 'svg': '🖼️',
		'mp4': '🎬', 'avi': '🎬', 'mov': '🎬',
		'mp3': '🎵', 'wav': '🎵',
		'zip': '🗜️', 'rar': '🗜️', 'tar': '🗜️', 'gz': '🗜️',
		'html': '🌐', 'css': '🎨', 'js': '⚡', 'php': '🐘', 'py': '🐍',
		'cpp': '⚙️', 'c': '⚙️', 'java': '☕',
		'json': '📋', 'xml': '📋',
		'csv': '📊', 'xls': '📊', 'xlsx': '📊'
	}
	return icons.get(extension.lower(), '📁')

def format_file_size(bytes_size):
	if bytes_size >= 1073741824:
		return f"{bytes_size / 1073741824:.2f} GB"
	elif bytes_size >= 1048576:
		return f"{bytes_size / 1048576:.2f} MB"
	elif bytes_size >= 1024:
		return f"{bytes_size / 1024:.2f} KB"
	else:
		return f"{bytes_size} bytes"

def log_error(message):
	try:
		with open('script.log', 'a') as log_file:
			timestamp = datetime.now().isoformat()
			log_file.write(f"{timestamp} {message}\n")
	except:
		pass  # Ignore logging errors

# Handle file upload
if request_method == 'POST' and content_length > 0:
	try:
		# Create FieldStorage instance to parse form data
		form = cgi.FieldStorage()
		
		if 'file' in form:
			fileitem = form['file']
			
			if fileitem.filename:
				# Sanitize filename
				filename = os.path.basename(fileitem.filename)
				filename = re.sub(r'[^a-zA-Z0-9._-]', '', filename)
				
				if not filename:
					filename = f'uploaded_file_{int(datetime.now().timestamp())}'
				
				# Get file extension
				file_extension = os.path.splitext(filename)[1][1:].lower()
				
				# Handle duplicate filenames
				base_name, ext = os.path.splitext(filename)
				counter = 0
				new_filename = filename
				target_path = os.path.join(upload_dir, new_filename)
				
				while os.path.exists(target_path):
					counter += 1
					new_filename = f"{base_name}({counter}){ext}"
					target_path = os.path.join(upload_dir, new_filename)
				
				filename = new_filename
				
				# Save uploaded file
				try:
					with open(target_path, 'wb') as f:
						if hasattr(fileitem.file, 'read'):
							shutil.copyfileobj(fileitem.file, f)
						else:
							f.write(fileitem.file)
					
					uploaded = True
					filesize = os.path.getsize(target_path)
					exit_code = '201'
					
				except Exception as e:
					error_message = 'Unable to save file'
					exit_code = '502'
			else:
				error_message = 'No file uploaded'
				exit_code = '400'
		else:
			error_message = 'No file field found'
			exit_code = '400'
			
	except Exception as e:
		error_message = f'Upload processing error: {str(e)}'
		exit_code = '502'
		
elif request_method != 'POST':
	error_message = 'Invalid request method'
	exit_code = '405'

if error_message:
	log_error(error_message)

# Get existing files in upload directory
existing_files = []
if os.path.isdir(upload_dir):
	try:
		for file in os.listdir(upload_dir):
			file_path = os.path.join(upload_dir, file)
			if os.path.isfile(file_path):
				existing_files.append(file)
	except:
		pass

# Build HTML content
html_content = f'''<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1" />
	<title>File Upload</title>
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

if uploaded:
	html_content += f'''
		<div class="file-icon" style="font-size: 4rem; margin-bottom: 1rem;">
			{get_file_icon(file_extension)}
		</div>
		<h1 class="title">File "{html.escape(filename)}" successfully uploaded!</h1>
		<p class="subtitle">
			Size: {format_file_size(filesize)}
			{f"| Type: {file_extension.upper()}" if file_extension else ""}
		</p>'''
else:
	html_content += '''
		<h1 class="title">Upload a File</h1>
		<p class="subtitle">Choose a file to upload to the server.</p>
		
		<form method="POST" enctype="multipart/form-data" style="margin: 2rem 0;">
			<div style="display: flex; gap: 1rem; align-items: center;">
				<input type="file" name="file" required style="flex-grow: 1; padding: 12px 18px; border: 2px solid #e2e8f0; border-radius: 14px; background: white; font-size: 0.95rem; font-family: inherit;">
				<button type="submit" class="button" style="margin-top: 0; height: fit-content;">Upload File</button>
			</div>
		</form>'''

# Display existing files
if existing_files:
	html_content += '''
		<div style="margin-top: 3rem; padding-top: 2rem; border-top: 1px solid rgba(255,255,255,0.2);">
			<h2 style="color: #64748b; margin-bottom: 1.5rem; text-align: center;">Files in Upload Directory</h2>
			<div style="display: grid; grid-template-columns: repeat(auto-fill, minmax(120px, 1fr)); gap: 1rem; margin-bottom: 2rem;">'''
	
	for file in existing_files:
		file_ext = os.path.splitext(file)[1][1:].lower()
		file_path = os.path.join(upload_dir, file)
		try:
			file_size = os.path.getsize(file_path)
		except:
			file_size = 0
		display_name = file[:12] + '...' if len(file) > 15 else file
		
		html_content += f'''
				<div style="position: relative; text-align: center; padding: 1rem; background: rgba(255,255,255,0.1); border-radius: 12px; backdrop-filter: blur(10px);">
					<form method="POST" action="delete.py" style="position: absolute; top: 8px; right: 8px; margin: 0;">
						<input type="hidden" name="filename" value="{html.escape(file)}">
						<input type="hidden" name="_method" value="DELETE">
						<button type="submit" 
								style="background: rgba(255,0,0,0.7); color: white; border: none; border-radius: 50%; width: 20px; height: 20px; font-size: 12px; cursor: pointer; display: flex; align-items: center; justify-content: center; font-weight: bold;">
							×
						</button>
					</form>
					
					<div style="font-size: 1.8rem; margin-bottom: 0.5rem;">
						{get_file_icon(file_ext)}
					</div>
					<div style="color: #64748b; font-size: 0.75rem; font-weight: 600; margin-bottom: 0.3rem;" title="{html.escape(file)}">
						{html.escape(display_name)}
					</div>
					<div style="color: #94a3b8; font-size: 0.65rem; font-weight: 600;">
						{format_file_size(file_size)}
					</div>
				</div>'''
	
	html_content += '''
			</div>
		</div>'''

html_content += '''
		<a href="/index.html" class="button">Back to Home</a>
	</div>
</body>
</html>'''

# Calculate content length
content_length = len(html_content.encode('utf-8'))

# Send exit_code
print(f"{exit_code}\r\n", end="")
print("\r")

# Send content
print(html_content)

