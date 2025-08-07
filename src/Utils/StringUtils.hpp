/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StringUtils.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jalombar <jalombar@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/07 14:04:56 by jalombar          #+#    #+#             */
/*   Updated: 2025/08/07 14:18:54 by jalombar         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include "includes/Webserv.hpp"

namespace su {

/**
 * Convert any numeric type to string
 * Equivalent to std::to_string from C++11
 */
template <typename T> std::string to_string(const T &value) {
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

/**
 * Convert string to numeric type
 * Returns true if conversion was successful, false otherwise
 */
template <typename T> bool from_string(const std::string &str, T &result) {
	std::istringstream iss(str);
	iss >> result;
	return !iss.fail() && iss.eof();
}

/**
 * Convert string to numeric type with default value
 * Returns the converted value or default_value if conversion fails
 */
template <typename T> T from_string_or_default(const std::string &str, const T &default_value) {
	T result;
	if (from_string(str, result)) {
		return result;
	}
	return default_value;
}

/**
 * Trim whitespace from the beginning of a string
 */
inline std::string ltrim(const std::string &str) {
	std::string result = str;
	result.erase(result.begin(), std::find_if(result.begin(), result.end(),
	                                          std::not1(std::ptr_fun<int, int>(std::isspace))));
	return result;
}

/**
 * Trim whitespace from the end of a string
 */
inline std::string rtrim(const std::string &str) {
	std::string result = str;
	result.erase(std::find_if(result.rbegin(), result.rend(),
	                          std::not1(std::ptr_fun<int, int>(std::isspace)))
	                 .base(),
	             result.end());
	return result;
}

/**
 * Trim whitespace from both ends of a string
 */
inline std::string trim(const std::string &str) { return ltrim(rtrim(str)); }

/**
 * Convert string to lowercase
 */
inline std::string to_lower(const std::string &str) {
	std::string result = str;
	std::transform(result.begin(), result.end(), result.begin(),
	               static_cast<int (*)(int)>(std::tolower));
	return result;
}

/**
 * Convert string to uppercase
 */
inline std::string to_upper(const std::string &str) {
	std::string result = str;
	std::transform(result.begin(), result.end(), result.begin(),
	               static_cast<int (*)(int)>(std::toupper));
	return result;
}

/**
 * Check if string starts with given prefix
 */
inline bool starts_with(const std::string &str, const std::string &prefix) {
	if (prefix.length() > str.length()) {
		return false;
	}
	return str.substr(0, prefix.length()) == prefix;
}

/**
 * Check if string ends with given suffix
 */
inline bool ends_with(const std::string &str, const std::string &suffix) {
	if (suffix.length() > str.length()) {
		return false;
	}
	return str.substr(str.length() - suffix.length()) == suffix;
}

/**
 * Split string by delimiter
 */
inline std::vector<std::string> split(const std::string &str, char delimiter) {
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream token_stream(str);

	while (std::getline(token_stream, token, delimiter)) {
		tokens.push_back(token);
	}

	return tokens;
}

/**
 * Split string by delimiter string
 */
inline std::vector<std::string> split(const std::string &str, const std::string &delimiter) {
	std::vector<std::string> tokens;
	size_t start = 0;
	size_t end = 0;

	while ((end = str.find(delimiter, start)) != std::string::npos) {
		tokens.push_back(str.substr(start, end - start));
		start = end + delimiter.length();
	}

	tokens.push_back(str.substr(start));
	return tokens;
}

/**
 * Join vector of strings with delimiter
 */
inline std::string join(const std::vector<std::string> &strings, const std::string &delimiter) {
	if (strings.empty()) {
		return "";
	}

	std::ostringstream oss;
	oss << strings[0];

	for (size_t i = 1; i < strings.size(); ++i) {
		oss << delimiter << strings[i];
	}

	return oss.str();
}

/**
 * Replace all occurrences of 'from' with 'to' in string
 */
inline std::string replace_all(const std::string &str, const std::string &from,
                               const std::string &to) {
	if (from.empty()) {
		return str;
	}

	std::string result = str;
	size_t start_pos = 0;

	while ((start_pos = result.find(from, start_pos)) != std::string::npos) {
		result.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}

	return result;
}

/**
 * Check if string contains only whitespace characters
 */
inline bool is_whitespace(const std::string &str) {
	return str.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
}

/**
 * Check if string contains substring
 */
inline bool contains(const std::string &str, const std::string &substring) {
	return str.find(substring) != std::string::npos;
}

/**
 * Pad string to specified length with given character (left padding)
 */
inline std::string pad_left(const std::string &str, size_t length, char pad_char = ' ') {
	if (str.length() >= length) {
		return str;
	}
	return std::string(length - str.length(), pad_char) + str;
}

/**
 * Pad string to specified length with given character (right padding)
 */
inline std::string pad_right(const std::string &str, size_t length, char pad_char = ' ') {
	if (str.length() >= length) {
		return str;
	}
	return str + std::string(length - str.length(), pad_char);
}

/**
 * Format string with printf-style formatting
 * Template version for type safety
 */
template <typename T> std::string format(const std::string &format_str, const T &value) {
	std::ostringstream oss;
	oss << value;

	// Simple replacement of %s with the value
	std::string result = format_str;
	size_t pos = result.find("%s");
	if (pos != std::string::npos) {
		result.replace(pos, 2, oss.str());
	}
	return result;
}

/**
 * Reverse a string
 */
inline std::string reverse(const std::string &str) {
	std::string result = str;
	std::reverse(result.begin(), result.end());
	return result;
}

/**
 * Count occurrences of substring in string
 */
inline size_t count_occurrences(const std::string &str, const std::string &substring) {
	if (substring.empty()) {
		return 0;
	}

	size_t count = 0;
	size_t pos = 0;

	while ((pos = str.find(substring, pos)) != std::string::npos) {
		++count;
		pos += substring.length();
	}

	return count;
}

/**
 * Extract substring between two delimiters
 */
inline std::string extract_between(const std::string &str, const std::string &start_delim,
                                   const std::string &end_delim) {
	size_t start_pos = str.find(start_delim);
	if (start_pos == std::string::npos) {
		return "";
	}

	start_pos += start_delim.length();
	size_t end_pos = str.find(end_delim, start_pos);

	if (end_pos == std::string::npos) {
		return str.substr(start_pos);
	}

	return str.substr(start_pos, end_pos - start_pos);
}

/**
 * Case-insensitive string comparison
 */
inline bool iequals(const std::string &str1, const std::string &str2) {
	return to_lower(str1) == to_lower(str2);
}

} // namespace su

#endif // STRINGUTILS_HPP
