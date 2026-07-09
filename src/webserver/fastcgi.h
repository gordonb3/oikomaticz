#pragma once
#ifndef HTTP_FASTCGI_H
#define HTTP_FASTCGI_H

#include "webserver/request_handler.h"
#include "webserver/IWebServerLogger.h"
#include <boost/logic/tribool.hpp>

#include "webserver/reply.h"
#include "webserver/request.h"
#include "webserver/server_settings.h"


namespace http {
namespace server {

class fastcgi_parser
{
public:
	static bool handlePHP(const server_settings &settings, const std::string &script_path, const request &req, reply &rep, modify_info &mInfo, const WebServerLogger &logger = nullptr);
	static uint16_t request_id_;
};

} //namespace server
} //namespace http

#endif //HTTP_FASTCGI_H

