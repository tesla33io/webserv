/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/27 14:09:28 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/29 13:45:04 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_HPP
#define CGI_HPP

# include "../../includes/types.hpp"
# include "../../includes/webserv.hpp"
# include "../Logger/Logger.hpp"
# include "../HttpServer/HttpServer.hpp"
#include "../HttpServer/Response.hpp"

namespace CGIUtils {
std::string get_interpreter(std::string &path);
std::string extract_content_type(std::string &cgi_headers);
void send_cgi_response(std::string &cgi_output, int clfd);
bool send_normal_resp(int reading_pipe, int clfd, pid_t pid);
bool handle_CGI_request(ClientRequest &request, int fd);
void send_chunk(int clfd, const char *data, size_t size);
bool send_cgi_headers(int clfd, std::string &first_chunk, std::string &remaining_data);
bool send_chunked_resp(int reading_pipe, int clfd, pid_t pid);
} // namespace CGIUtils

#endif
