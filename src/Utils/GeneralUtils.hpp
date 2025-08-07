/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   GeneralUtils.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:03:32 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:19:11 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef GENERALUTILS_HPP
#define GENERALUTILS_HPP

#include "includes/Webserv.hpp"

inline std::string describeEpollEvents(uint32_t ev) {
	std::vector<std::string> event_names;

	if (ev & EPOLLIN)
		event_names.push_back("EPOLLIN");
	if (ev & EPOLLOUT)
		event_names.push_back("EPOLLOUT");
	if (ev & EPOLLPRI)
		event_names.push_back("EPOLLPRI");
	if (ev & EPOLLERR)
		event_names.push_back("EPOLLERR");
	if (ev & EPOLLHUP)
		event_names.push_back("EPOLLHUP");
	if (ev & EPOLLRDHUP)
		event_names.push_back("EPOLLRDHUP");
	if (ev & EPOLLONESHOT)
		event_names.push_back("EPOLLONESHOT");
	if (ev & EPOLLET)
		event_names.push_back("EPOLLET");

	if (event_names.empty()) {
		return "0";
	}

	std::string result = event_names[0];
	for (size_t i = 1; i < event_names.size(); ++i) {
		result += "|" + event_names[i];
	}

	return result;
}

inline size_t findCRLF(const std::string &buffer, size_t start_pos = 0) {
	return buffer.find("\r\n", start_pos);
}

#endif
