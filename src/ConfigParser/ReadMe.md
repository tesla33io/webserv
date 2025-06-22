Configuration Parser 
To handle NGINX-style configuration files. 

The configuration is:
- Has a limited, known depth (rarely deeper than 3 levels)
- Declarative, not expression-based
- Structured with few block types
- Does not require recursion, operator precedence, or dynamic AST traversal

We could use a nested struct + std::vector model rather than a generic tree. 
It is simpler, more efficient, and easier to maintain.
But we go for the AST to be more extensive



Core Block Types

 Block 				Description
events		Connection-level settings (worker threads, etc.)
 http		All HTTP config: servers, routes, etc.
stream		For TCP/UDP load balancing (not used in webserv)
 mail		For mail protocols (SMTP, IMAP, POP3)
(global)	Directives like worker_processes, pid, etc. that are outside any block



Pitfalls

Nested blocks:
Watch for nested {} pairs; use a stack to track open/close.

Multi-line values:
Some directives (e.g., log_format) allow multi-line values.

Include directive:
include can pull in other files. Consider supporting this.

Edge cases:
Semicolons inside quotes (e.g., in regex or strings)
Escaped characters


Exhaustive directives list: 
- the valid directive arrays contains almost 100 directives. this should be enough.
But rn there are 878 directives on the nginx page. And the list can be updated.
-> the bet solution would be to populate the array directly from the website (scraping script?).

