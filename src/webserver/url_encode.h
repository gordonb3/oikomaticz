#pragma once
#include <string>

namespace http {
namespace server {

// URL encoding/decoding utilities (RFC 3986 / ISO-8859-1 compatible).

std::string URLEncode(const std::string &src);
std::string URLDecode(const std::string &src);

} // namespace server
} // namespace http
