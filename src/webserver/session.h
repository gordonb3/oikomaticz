#pragma once

#include <cstdint>
#include <ctime>
#include <string>

namespace http {
namespace server {

enum _eUserRights
{
	URIGHTS_VIEWER   = 0,
	URIGHTS_SWITCHER = 1,
	URIGHTS_ADMIN    = 2,
	URIGHTS_NONE     = 254,
	URIGHTS_CLIENTID = 255
};

typedef struct _tWebEmSession
{
	std::string id;
	std::string remote_host;
	std::string local_host;
	std::string remote_port;
	std::string local_port;
	std::string auth_token;
	std::string username;
	time_t      expires      = 0;
	uint16_t    reply_status = 200; // corresponds to reply::ok
	_eUserRights rights       = URIGHTS_NONE;
	bool        rememberme   = false;
	bool        isnew        = false;
	bool        istrustednetwork = false;
} WebEmSession;

} // namespace server
} // namespace http
