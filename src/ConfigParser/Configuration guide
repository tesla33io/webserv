# # # # # # # # # # Web Server Configuration Guide # # # # # # # # # # # # 

This guide explains how to write configuration files for the web server. The configuration uses a nginx-like syntax with hierarchical blocks and directives.

# # # # Basic Structure # # # # 
http {
    server {
        # Server-level directives
        location /path {
            # Location-specific directives
        }
    }
}

# # # # # Syntax Rules # # # # # 

# Comments
Use # for comments
Comments can be on their own line or at the end of a directive
Everything after # is ignored
example:
#This is a comment
listen 8080;  # This is also a comment

# Statements
All directives must end with a semicolon ;
Blocks start with { and end with }

# Whitespace
Extra spaces and blank lines are ignored
Indentation is optional but recommended for readability


# # # # Directive Reference # # # # 

# HTTP Block
The top-level container for all server configurations.
http {
    # All server blocks go here
}

# Server Block
Defines a virtual server with its own configuration.
server {
    # Server-specific directives
}

# # Server-Level Directives # # 

# listen
Syntax: listen [host:]port;
Context: server
Required: No (only one per server)
Defines the IP address and port for the server to listen on.
Default: 0.0.0.0:8080
listen 8080;                    # Listen on all interfaces, port 8080 (0.0.0.0:8080)
listen :8080;                   # Same as above (0.0.0.0:8080)
listen 127.0.0.1:8080;          # Listen on localhost only
listen 192.168.1.100:9000;      # Listen on specific IP
listen 127.0.0.1                # Invalid 
Valid ports: 1-65535

# client_max_body_size
Syntax: client_max_body_size size;
Context: server
Default: 1MB (1048576 bytes)
Sets the maximum allowed size of client request body.
client_max_body_size 1024;      # 1024 bytes
client_max_body_size 512K;      # 512 kilobytes
client_max_body_size 2M;        # 2 megabytes
client_max_body_size 1G;        # 1 gigabyte
client_max_body_size 0;         # infinite
Suffixes: K/k (kilobytes), M/m (megabytes), G/g (gigabytes)

# error_page
Syntax: error_page code1 [code2 ...] uri;
Context: server
Repeatable: Yes
Defines custom error pages for HTTP status codes.
error_page 404 404.html;
error_page 500 502 503 error/50x.html;
error_page 403 /forbidden.html;
Valid codes: the common error codes ranging 400-599


# # Server or Location Level Directives # #
These directives can be used at server level (inherited by all locations) or overridden at location level.

# root 
Syntax: root path;
Context: server, location
Default: None
Sets the root directory for requests.
root /var/www;
root /var/www/html;
Rules: Must start with /, check for invalid character (no '.' allowed)

# allowed_methods
Syntax: allowed_methods method1 [method2 method3];
Context: server, location
Specifies which HTTP methods are allowed.
allowed_methods GET;
allowed_methods GET POST;
allowed_methods GET POST DELETE;
Valid methods: GET, POST, DELETE
Directive not present -> all methods are allowed

# upload_path
Syntax: upload_path path;
Context: server, location
Sets the directory for file uploads.
upload_path /var/uploads;
upload_path /tmp/uploads;
Rules: Must start with /, check for invalid character (no '.' allowed)

# cgi_ext
Syntax: cgi_ext extension1 interpreter1 [extension2 interpreter2 ...];
Context: server, location
Maps file extensions to CGI interpreters.
cgi_ext .py /usr/bin/python3;
cgi_ext .py /usr/bin/python3 .php /usr/bin/php;
Supported extensions: .py, .php
 Interpreter location rules ???

# index
Syntax: index filename;
Context: server, location
Sets the default file to serve for directory requests.
index index.html;
index main.html;
index home.php;
Rules:
Cannot contain quotes


# # Location-Only Directives # # 

# location
Syntax: location path { ... }
Context: server
Defines configuration for specific URL paths.
location / {
    # Configuration for all paths
}
location /images/ {
    # Configuration for /images and /images/*
}
location /api/v1/ {
    # Configuration for /api/v1 and /api/v1/*
}
Rules:
Path must start and ends with '/'
Cannot contain .. or quotes
Cannot be the same path as an existing block

# autoindex
Syntax: autoindex on|off;
Context: location
Enables or disables directory listing.
location / {
    autoindex on;   # Show directory contents
}
location /private {
    autoindex off;  # Don't show directory contents
}

# return
Syntax: return code [URI|URL] or return [URL];
Context: location
Returns a specific HTTP status code and redirects. 
location /old-path {
    return 301 /new-path;           # Permanent redirect
}
location /temp {
    return 302 https://example.com; # Temporary redirect
}
location /temp {
    return https://example.com; # Temporary redirect - code set to 302
}
Rules:
Status codes: 300, 301, 302, 303, 307, 308



# # # # Configuration Examples # # # # 

# Simple Web Server
http {
    server {
        listen 8080;
        root /var/www;
        index index.html;
        
        location / {
            autoindex on;
        }
    }
}

# Multi-Site Configuration
http {
    server {
        listen 80;
        root /var/www/site1;
        error_page 404 /404.html;
        
        location / {
            allowed_methods GET POST;
            autoindex off;
        }
        
        location /uploads/ {
            upload_path /var/uploads/site1;
            allowed_methods POST;
        }
    }
    
    server {
        listen 8080;
        root /var/www/site2;
        client_max_body_size 10M;
        
        location / {
            autoindex on;
        }
        
        location /api/ {
            cgi_ext .py /usr/bin/python3;
            allowed_methods GET POST;
        }
    }
}

# API Server with CGI
http {
    server {
        listen 3000;
        root /var/api;
        client_max_body_size 5M;
        allowed_methods GET POST DELETE;
        cgi_ext .py /usr/bin/python3 .php /usr/bin/php;
        
        location / {
            autoindex off;
        }
        
        location /v1/ {
            upload_path /var/api/uploads;
        }
        
        location /admin/ {
            return 401;  # Require authentication
        }
        
        location /health/ {
            return 200;  # Health check endpoint
        }
    }
}

# # # # Inheritance Rules # # # # 
Settings defined at the server level are inherited by all locations unless overridden:
server {
    root /var/www;              # Inherited by all locations
    allowed_methods GET POST;   # Inherited by all locations
    index index.html;           # Inherited by all locations
    
    location / {
        # Uses: root=/var/www, methods=GET POST, index=index.html
        autoindex on;
    }
    
    location /api {
        allowed_methods POST;   # Overrides server setting
        # Still uses: root=/var/www, index=index.html
    }
}

# # # # Common Mistakes # # # # 

Missing semicolons
listen 8080  # ❌ Missing semicolon
listen 8080; # ✅ Correct

Invalid paths
root /var/www/;    # ❌ Ends with slash
root /var/www;     # ✅ Correct

location images {  # ❌ Doesn't start with /
location /images { # ✅ Correct

Invalid return usage
return 301;                    # ❌ Redirect without URL
return 301 /new-path;          # ✅ Correct

return 404 /not-found;         # ❌ Non-redirect with URL
return 404;                    # ✅ Correct

Duplicate non-repeatable directives
server {
    listen 8080;
    listen 9000;  # ❌ listen cannot be repeated
}

Invalid method names
allowed_methods GET PUT;     # ❌ PUT not supported
allowed_methods GET POST;    # ✅ Correct


# # # # Validation # # # # 
Syntax checking (brackets, semicolons)
Context validation (directives in correct blocks)
Argument count validation
Value format validation (IPs, ports, paths, etc.)
Duplication checking for non-repeatable directives

Error messages will indicate the line number and specific issue found.