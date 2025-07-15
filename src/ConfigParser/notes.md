////////////////////////////////////////////////////////////////////////////////
Configuration Parser 
To handle NGINX-style configuration files. The configuration file consists of directives and their parameters. Simple (single‑line) directives end with a semicolon ( ; ). Other directives act as containers” which group together related directives. Containers are enclosed in curly braces ( {} ) and are often referred to as blocks. Here are some examples of simple directives.

////////////////////////////////////////////////////////////////////////////////
TODO: directive validation: add uri syntax validation?
TODO: directive validation: update the list of method and CGI extension we wanna accept
TODO: directive validation: add tests
TODO: directive validation: add the default index files?
other: check the use of "", '', change the line approach to a character approach?

////////////////////////////////////////////////////////////////////////////////
to compare with NGINX : docker run --rm -v $(pwd)/mini.conf:/etc/nginx/nginx.conf:ro nginx nginx -t

////////////////////////////////////////////////////////////////////////////////
Configuration specifications

	- "events": 
	one block events just to be able to compare directly with nginx. No args, no directive allowed.

	- "http": 
	the block containing the server blocks. Again, mostly to compare with nginx. Does not accept global directives. No args.
	Possible Improvement: Accept include, log_format, or access_log for realism?

	- "server": 
	server blocks. No argument. Accepts local directive (server_names, listen, location) and global directives (error_page, client_max_body_size, return, root, autoindex, index, chunked_transfer_encoding).
	Multiple server blocks can be specified.
	Amelioration: check uniqueness of listen: + server_name?

	- "server_name"
	At least one argument, up to size_t server names. DEFAULT: empty
	Amelioration: implement wildcard / regex?

	- "listen":
	Accepts one argument: 8080 or :80 or x.x.x.x:8080.
	DEFAULT: set the address to 0.0.0.0 and the port to 8080.
	Amelioration: accept ipv6, accept only ip (no port)

  	- "location": 
	location blocks. Must have one argument (the path). Must be in a server block.
	Multiple location blocks can be specified.
	Amelioration: handle modifiers?

	- "error_page": 
	can be global (server block), or local (location block). At least two arguments. All but the last argument must be unsigned int. This directive can be repeated as long as the error codes differ.
	Amelioration:  handling redirection? (error_page 404 = @fallback;)?

	- "root": 
	Sets the root directory for requests. Can be global (server block), or local (location block).
	Syntax:	root path; 
	Default: root "";
	
  	- "client_max_body_size":
	can be global (server block), or local (location block). Takes one argument: the size in bytes. (int)
	Possible Improvement: accept the unit (ex: 5M. NGinX: Sizes can be specified in bytes, kilobytes, or megabytes using the following suffixes: k and K for kilobytes, m and M for megabytes)?

	- "limit_except":
	between 1 and 4 arguments: GET, HEAD, DELETE, POST. 

	- "return":
	can be global (server block), or local (location block). Takes one to 2 arguments : an error code and opt an URI

	- "autoindex":
	can be global (server block), or local (location block). Takes one argument: on or off.
	DEFAULT: off

	- "cgi_ext":
	In a location block. Takes one to size_t arguments. 
	Check: must be accepted: .py, .php atm.

	- "index":
	can be global (server block), or local (location block). Takes one to size_t arguments.

	- "alias":
	In a location block. Takes one argument.
	Attention: alias has different path resolution semantics than root. If both root and alias are set for a location, might need to error/conflict warning. Not checked

	- "chunked_transfer_encoding":
	can be global (server block), or local (location block). Takes one argument: on or off.
	DEFAULT: off

////////////////////////////////////////////////////////////////////////////////
Standard HTTP code classes:
1xx: Informational (rarely used in error_page/return)
2xx: Success (e.g., return 200 foo, sometimes used with error_page 404 =200 /empty.gif;)
3xx: Redirection (e.g., 301, 302, 307, 308)
4xx: Client error (e.g., 400, 403, 404, 405)
5xx: Server error (e.g., 500, 502, 503, 504, 505)


////////////////////////////////////////////////////////////////////////////////
// NGINX ERROR MESSAGES

[emerg] unexpected "}" in /etc/nginx/nginx.conf:45
→ You closed a ConfigNode that wasn’t open.

[emerg] unexpected end of file, expecting "}" in /etc/nginx/nginx.conf:67
→ You forgot to close a ConfigNode (e.g., server { with no matching }).

[emerg] invalid number of arguments in "listen" directive in /etc/nginx/nginx.conf:20
→ The directive was given too many or too few arguments.

[emerg] directive "server" is not terminated by ";" in /etc/nginx/nginx.conf:21
→ You forgot the semicolon at the end of a directive.

[emerg] "listen" directive is not allowed here in /etc/nginx/nginx.conf:13
→ You probably put listen outside of a server ConfigNode.

[emerg] duplicate "listen" directive in /etc/nginx/nginx.conf:25
→ You defined listen more than once in a server ConfigNode without using default_server, etc.

[emerg] a duplicate default server for 0.0.0.0:80 in /etc/nginx/nginx.conf:35
→ You have more than one default_server for the same address/port.

[emerg] open() "/etc/nginx/mime.types" failed (2: No such file or directory)
→ You referenced a file that doesn’t exist (e.g., with include).

[emerg] unknown directive "slurp" in /etc/nginx/nginx.conf:12
→ You used a directive that NGINX doesn’t recognize (typo or unsupported module).

[emerg] invalid port in "listen" directive in /etc/nginx/nginx.conf:16
→ You gave a non-numeric or invalid port.
