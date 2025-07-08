////////////////////////////////////////////////////////////////////////////////
Configuration Parser 
To handle NGINX-style configuration files. 
////////////////////////////////////////////////////////////////////////////////

Core Block Types
Block 				Description
events		  Connection-level settings (worker threads, etc.)
http		    All HTTP config: servers, routes, etc.
stream	  	For TCP/UDP load balancing (not used in webserv) 
mail		    For mail protocols (SMTP, IMAP, POP3) (not used in webserv) 
(global)	  Directives like worker_processes, pid, etc. that are outside any block

Parsing: 
AST parsing with line check (error message will be printed with the line number)
Then parsed into a ServerConfig struct for easy acces to infos.


to do:
- directive validation
- set up the DEFAULT VALUES 
- tests (subject: You must provide configuration files and default files to test
  and demonstrate that every feature works during the evaluation)
- accept different config struct 
- handle the ~ etc as second arg for locations (location = /status , location ~* \.(gif))

TESTS : to compare with NGINX (docker run) :
// docker run --rm -v $(pwd)/mini.conf:/etc/nginx/nginx.conf:ro nginx nginx -t



/*//////////////////////////////////////////////////////////////////////////////
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

/*/////////////////////////////////////////////////////////////////////////////*

