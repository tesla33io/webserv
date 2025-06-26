/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/19 14:48:54 by jalombar          #+#    #+#             */
/*   Updated: 2025/06/26 10:23:24 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils.hpp"

std::string GeneralUtils::to_upper(std::string &s) {
	for (size_t i = 0; i < s.length(); ++i)
		s[i] = std::toupper(s[i]);
	return (s);
}

std::string GeneralUtils::to_lower(std::string &s) {
	for (size_t i = 0; i < s.length(); ++i)
		s[i] = std::tolower(s[i]);
	return (s);
}
