#pragma once
#include <cstdarg>
#include <memory>

namespace http {
namespace server {

enum class LogLevel
{
	Error,
	Status,
	Debug
};

enum class DebugCategory
{
	WebServer,
	Auth
};

class IWebServerLogger
{
public:
	virtual ~IWebServerLogger() = default;
	virtual void Log(LogLevel level, const char *fmt, ...) = 0;
	virtual void Debug(DebugCategory category, const char *fmt, ...) = 0;

	// Apache Combined Log Format access logging (used by connection.cpp)
	virtual bool IsAccessLogEnabled() { return false; }
	virtual void AccessLog(const char *fmt, ...) {}
};

using WebServerLogger = std::shared_ptr<IWebServerLogger>;

} // namespace server
} // namespace http
