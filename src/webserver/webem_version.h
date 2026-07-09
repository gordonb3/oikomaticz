// libwebem — Lightweight C++17 HTTP/WebSocket server library
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#define WEBEM_VERSION_MAJOR 1
#define WEBEM_VERSION_MINOR 31
#define WEBEM_VERSION_PATCH 0
#define WEBEM_VERSION_STRING "1.31.0"

// Build date (set at compile time)
#define WEBEM_BUILD_DATE __DATE__
#define WEBEM_BUILD_TIME __TIME__

namespace http {
namespace server {

    /// Returns the library version string (e.g. "1.31.0")
    inline const char* webem_version() { return WEBEM_VERSION_STRING; }

    /// Returns the library build date (e.g. "Feb 23 2026")
    inline const char* webem_build_date() { return WEBEM_BUILD_DATE; }

    /// Returns the library build time (e.g. "14:30:00")
    inline const char* webem_build_time() { return WEBEM_BUILD_TIME; }

} // namespace server
} // namespace http
