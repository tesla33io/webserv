/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   GeneralUtils.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 14:45:55 by jalombar          #+#    #+#             */
/*   Updated: 2025/07/18 11:44:57 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
#define UTILS_HPP

# include "../../includes/webserv.hpp"

namespace GeneralUtils {
	inline std::string to_upper(std::string &s) {
		for (size_t i = 0; i < s.length(); ++i)
			s[i] = std::toupper(s[i]);
		return (s);
	}
	inline std::string to_lower(std::string &s) {
		for (size_t i = 0; i < s.length(); ++i)
			s[i] = std::tolower(s[i]);
		return (s);
	}
} // namespace GeneralUtils

#endif
