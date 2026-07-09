#include "webem_stdafx.h"
#include "url_encode.h"
#include <array>
#include <cmath>
#include <string>

// URL encoding/decoding implementation.
// Based on ISO-8859-1 compatible percent-encoding.

namespace http {
namespace server {

namespace {

// Characters that must be percent-encoded in a URL component.
constexpr const char* kUnsafeChars = "\"<>%\\^[]`+$,@:;/!#?=&";

constexpr auto kHexChars = std::array<char, 16>{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F'
};

// Returns the two-digit uppercase hex string for a single byte value.
// Handles ISO-8859-1 range (0-255).
static std::string ByteToHex(char c)
{
    int val = static_cast<int>(c);
    if (val < 0)
        val += 256; // treat as unsigned byte

    std::string hex;
    hex += kHexChars[(val >> 4) & 0x0F];
    hex += kHexChars[val & 0x0F];
    return hex;
}

// Returns true if the character needs to be percent-encoded.
static bool NeedsEncoding(char c)
{
    const int ascii = static_cast<int>(c);
    if (ascii <= 32 || ascii >= 123)
        return true;
    for (const char* p = kUnsafeChars; *p != '\0'; ++p)
    {
        if (*p == c)
            return true;
    }
    return false;
}

} // anonymous namespace

std::string URLEncode(const std::string &src)
{
    std::string encoded;
    encoded.reserve(src.size());
    for (char c : src)
    {
        if (!NeedsEncoding(c))
        {
            encoded += c;
        }
        else
        {
            encoded += '%';
            encoded += ByteToHex(c);
        }
    }
    return encoded;
}

std::string URLDecode(const std::string &src)
{
    std::string decoded;
    decoded.reserve(src.size());
    const size_t len = src.size();
    for (size_t i = 0; i < len; ++i)
    {
        if (src[i] == '%')
        {
            if (i + 2 >= len)
                return src; // malformed - return original
            auto hex_val = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                return -1;
            };
            int hi = hex_val(src[i + 1]);
            int lo = hex_val(src[i + 2]);
            if (hi < 0 || lo < 0)
                return {};
            decoded += static_cast<char>((hi << 4) | lo);
            i += 2;
        }
        else
        {
            decoded += src[i];
        }
    }
    return decoded;
}

} // namespace server
} // namespace http
