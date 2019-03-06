#pragma once
#include "typedef/common.hpp"

namespace device {
namespace media {
namespace status {

enum value {
	OFF = 0,
	ON,
	PAUSED,
	STOPPED,
	VIDEO,
	AUDIO,
	PHOTO,
	PLAYING,
	DISCONNECTED,
	SLEEPING,
	UNKNOWN,
	END
};

#ifdef INCLUDE_TYPEDEF_CODE
// tiny hack that allows us to keep these definitions together in a single file

const char *Description(const device::media::status::value Status)
{
	static const STR_TABLE_SINGLE Table[] =
	{
		{ OFF,		"Off" },
		{ ON,		"On" },
		{ PAUSED,	"Paused" },
		{ STOPPED,	"Stopped" },
		{ VIDEO,	"Video" },
		{ AUDIO,	"Audio" },
		{ PHOTO,	"Photo" },
		{ PLAYING,	"Playing" },
		{ DISCONNECTED,	"Disconnected" },
		{ SLEEPING,	"Sleeping" },
		{ UNKNOWN,	"Unknown" },
		{ 0, NULL, NULL }
	};
	return findTableIDSingle1(Table, Status);
}



#else // INCLUDE_TYPEDEF_CODE
const char *Description(const device::media::status::value Status);
#endif // INCLUDE_TYPEDEF_CODE


}; // namespace status
}; // namespace media
}; // namespace device

