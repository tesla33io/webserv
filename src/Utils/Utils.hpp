#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <sstream>

namespace utils {

	// int to string
	inline std::string toString(int value) {
		std::ostringstream oss;
		oss << value;
		return oss.str();
	}

	// double to string with fixed precision
	inline std::string toString(double value, int precision = 2) {
		std::ostringstream oss;
		oss.precision(precision);
		oss << std::fixed << value;
		return oss.str();
	}

	// bool to string
	inline std::string toString(bool value) {
		return value ? "true" : "false";
	}

	// any streamable type to string using templates
	template <typename T>
	std::string toString(const T& value) {
		std::ostringstream oss;
		oss << value;
		return oss.str();
	}

} 

#endif 
