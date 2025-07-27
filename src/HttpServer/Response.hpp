#ifndef RESPONSE_HPP

#define RESPONSE_HPP

#include <map>
#include <stdint.h>
#include <string>

class Response {
  public:
	std::string version;                        // HTTP/1.1
	uint16_t status_code;                       // e.g. 200
	std::string reason_phrase;                  // e.g. OK
	std::map<std::string, std::string> headers; // e.g. Content-Type: text/html
	std::string body;                           // e.g. <h1>Hello world!</h1>

	Response();
	explicit Response(uint16_t code);
	Response(uint16_t code, const std::string &response_body);

	inline void setStatus(uint16_t code);
	inline void setHeader(const std::string &name, const std::string &value);
	inline void setContentType(const std::string &ctype);
	inline void setContentLength(size_t length);
	std::string toString() const;
	std::string toShortString() const;
	void reset();

	// Factory methods for common responses
	static Response continue_();
	static Response ok(const std::string &body = "");
	static Response notFound();
	static Response internalServerError();
	static Response badRequest();
	static Response methodNotAllowed();

  private:
	std::string getReasonPhrase(uint16_t code) const;
	void initFromStatusCode(uint16_t code);
};

#endif /* end of include guard: RESPONSE_HPP */
