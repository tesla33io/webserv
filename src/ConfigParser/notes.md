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
AST parsing to be more extensive
Directives list: we create an array with the valid directives we handle. If a 
directive is not recognized -> error. Need to check which are relevant.
File locations: we check the existence of the files. Error if not exist


Edge cases (not handled): 
 - Multi-line values: Some directives (e.g., log_format) allow multi-line values. 
   -> probably not needed 
 - Include directive:
   include can pull in other files. -> probably not needed
 - Semicolons inside quotes, Escaped characters

 DEFAULT VALUES - handling where?
 CHUNK thing - to research


////////////////////////////////////////////////////////////////////////////////
// SUBJECT:

You must provide configuration files and default files to test and demonstrate that
every feature works during the evaluation.

In the configuration file, you should be able to:
• Choose the port and host of each ’server’.
• Set up the server_names or not.
• The first server for a host:port will be the default for this host:port (meaning it
will respond to all requests that do not belong to another server).
• Set up default error pages.
• Set the maximum allowed size for client request bodies.
• Set up routes with one or multiple of the following rules/configurations (routes
won’t be using regexp):
  ◦ Define a list of accepted HTTP methods for the route.
  ◦ Define an HTTP redirect.
  ◦ Define a directory or file where the requested file should be located (e.g.,
    if url /kapouet is rooted to /tmp/www, url /kapouet/pouic/toto/pouet is
    /tmp/www/pouic/toto/pouet).
  ◦ Enable or disable directory listing.
  ◦ Set a default file to serve when the request is for a directory.
  ◦ Execute CGI based on certain file extension (for example .php).
  ◦ Make it work with POST and GET methods.
  ◦ Allow the route to accept uploaded files and configure where they should be
    saved.


∗ Do you wonder what a CGI is?
∗ Because you won’t call the CGI directly, use the full path as PATH_INFO.
∗ Just remember that, for chunked requests, your server needs to unchunk
them, the CGI will expect EOF as the end of the body.
∗ The same applies to the output of the CGI. If no content_length is
returned from the CGI, EOF will mark the end of the returned data.
∗ Your program should call the CGI with the file requested as the first
argument.
∗ The CGI should be run in the correct directory for relative path file access.
∗ Your server should support at least one CGI (php-CGI, Python, and so
forth).

