/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/31 08:58:57 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/08 14:22:56 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_HPP
#define CGI_HPP

#include "includes/Webserv.hpp"
#include "includes/Types.hpp"
#include "src/ConfigParser/ConfigParser.hpp"
#include "src/Logger/Logger.hpp"
#include "src/HttpServer/Structs/Response.hpp"

class CGI {
  private:
	std::map<std::string, std::string> env_;
	std::string script_path_;
	std::string interpreter_;
	int output_fd_;
	pid_t pid_;

  public:
	CGI(ClientRequest &request, LocConfig *locConfig);
	~CGI(){};

	// ENV
	void setEnv(const std::string &key, const std::string &value);
	std::string getEnv(const std::string &key) const;
	void unsetEnv(const std::string &key);
	char **toEnvp() const;
	static void freeEnvp(char **envp);

	// Getters/Setters
	void setInterpreter(std::string &interpreter);
	const char *getInterpreter() const;
	const char *getScriptPath() const;
	void setPid(pid_t pid);
	pid_t getPid() const;
	void setOutputFd(int fd);
	int getOutputFd() const;

	// CGI handler
	void printCGIResponse(const std::string &cgi_output);
	std::string extractContentType(std::string &cgi_headers);
	bool cleanup();

	void sendCGIResponse(std::string &cgi_output, int clfd);
	bool sendNormalResp(CGI &cgi, int clfd);
	void sendChunk(int clfd, const char *data, size_t size);
	bool sendCGIHeaders(CGI &cgi, int clfd, std::string &first_chunk, std::string &remaining_data);
	bool sendChunkedResp(CGI &cgi, int clfd);
};

namespace CGIUtils {
bool runCGIScript(ClientRequest &req, CGI &cgi);
CGI *createCGI(ClientRequest &req, LocConfig *locConfig);
} // namespace CGIUtils

#endif
