//
// webem_utils.h
// ~~~~~~~~~~~~~
//
// Internal utility functions for the webserver library.
// These are self-contained and have no dependency on the application core.
//
#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <thread>
#include <sys/stat.h>

namespace http {
namespace server {
namespace utils {

    /// Wraps ::time() with the same signature.
    inline time_t webem_time(time_t* t = nullptr)
    {
        return ::time(t);
    }

    /// Splits 'input' on 'delimiter' and appends each token to 'results'.
    /// Consecutive delimiters produce empty-string tokens.
    void split_string(const std::string& input,
                      const std::string& delimiter,
                      std::vector<std::string>& results);

    /// Cross-platform localtime_r wrapper.
    /// On Windows uses localtime_s (reversed argument order); on POSIX uses
    /// localtime_r directly.  Returns 'result' on success, nullptr on error.
    inline struct tm* safe_localtime(const time_t* time, struct tm* result)
    {
#ifdef _WIN32
        if (localtime_s(result, time) == 0)
            return result;
        return nullptr;
#else
        return localtime_r(time, result);
#endif
    }

    /// Replaces all occurrences of replaceWhat in inoutstring with replaceWithWhat.
    inline void str_replace(std::string& inoutstring,
                            const std::string& replaceWhat,
                            const std::string& replaceWithWhat)
    {
        if (replaceWhat.empty())
            return;
        std::string::size_type pos = 0;
        while ((pos = inoutstring.find(replaceWhat, pos)) != std::string::npos)
        {
            inoutstring.replace(pos, replaceWhat.size(), replaceWithWhat);
            pos += replaceWithWhat.size();
        }
    }

    /// Converts inoutstring to upper case in place.
    inline void str_upper(std::string& inoutstring)
    {
        std::transform(inoutstring.begin(), inoutstring.end(),
                       inoutstring.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    }

    /// Returns a copy of s with leading and trailing whitespace removed.
    inline std::string trim_whitespace(const std::string& s)
    {
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return {};
        auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    /// In-place variant: trims leading and trailing whitespace from s.
    inline void trim_whitespace_inplace(std::string& s)
    {
        s = trim_whitespace(s);
    }

    /// Returns a random UUID v4 string (e.g. "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx").
    std::string generate_uuid();

    /// Returns true if the file at path exists and is accessible.
    inline bool file_exists(const char* path)
    {
        struct stat st;
        return (stat(path, &st) == 0);
    }

    /// Sets the OS-level name of the given thread handle.
    int set_thread_name(const std::thread::native_handle_type& thread, const char* name);

    /// Formats rawtime as an RFC 1123 HTTP date string.
    /// Returns a pointer to a thread-local static buffer; valid until the
    /// next call on the same thread.
    char* make_web_time(const time_t rawtime);

    /// Compute MD5 hash of InputString concatenated with Salt.
    /// Requires OpenSSL (libcrypto).
    std::string GenerateMD5Hash(const std::string& InputString, const std::string& Salt = "");

    /// Cross-platform gettimeofday replacement.
    /// On POSIX, delegates to ::gettimeofday(). On Windows, uses GetSystemTimeAsFileTime().
    /// Namespaced to avoid linker conflicts with consumer-provided implementations.
    int get_timeofday(struct timeval* tp);

} // namespace utils
} // namespace server
} // namespace http
