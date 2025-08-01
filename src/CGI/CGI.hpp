/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/31 08:58:57 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/01 11:45:10 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_HPP
#define CGI_HPP

#include "../../includes/types.hpp"
#include "../../includes/webserv.hpp"
#include "../HttpServer/Response.hpp"
#include "../Logger/Logger.hpp"

class CGI {
  private:
	std::map<std::string, std::string> env_;
	std::string script_path_;
	std::string interpreter_;
	int output_fd_;
	pid_t pid_;

  public:
	// CGI();
	CGI(ClientRequest &request);
	~CGI(){};

	// ENV
	void setEnv(const std::string &key, const std::string &value);
	std::string getEnv(const std::string &key) const;
	void unsetEnv(const std::string &key);
	char **toEnvp() const;
	static void freeEnvp(char **envp);

	// Getters/Setters
	void setInterpreter(std::string &path);
	const char *getInterpreter() const;
	const char *getScriptPath() const;
	void setPid(pid_t pid);
	pid_t getPid() const;
	void setOutputFd(int fd);
	int getOutputFd() const;

	// CGI handler
	void print_cgi_response(const std::string &cgi_output);
	std::string extract_content_type(std::string &cgi_headers);
	bool cleanup();

	void send_cgi_response(std::string &cgi_output, int clfd);
	bool send_normal_resp(CGI &cgi, int clfd);
	void send_chunk(int clfd, const char *data, size_t size);
	bool send_cgi_headers(CGI &cgi, int clfd, std::string &first_chunk,
	                      std::string &remaining_data);
	bool send_chunked_resp(CGI &cgi, int clfd);
};

namespace CGIUtils {
bool run_CGI_script(ClientRequest &req, CGI &cgi);
bool CGI_handler(ClientRequest &req, int clfd);
CGI *create_CGI(ClientRequest &req);
} // namespace CGIUtils

#endif
