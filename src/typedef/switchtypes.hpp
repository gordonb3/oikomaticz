#pragma once
#include "typedef/common.hpp"

namespace device {
namespace tswitch {
namespace type {

enum value {
	OnOff = 0,			//  0
	Doorbell,			//  1
	Contact,			//  2
	Blinds,				//  3
	X10Siren,			//  4
	SMOKEDETECTOR,			//  5
	BlindsInverted,			//  6
	Dimmer,				//  7
	Motion,				//  8
	PushOn,				//  9
	PushOff,			// 10
	DoorContact,			// 11
	Dusk,				// 12
	BlindsPercentage,		// 13
	VenetianBlindsUS,		// 14
	VenetianBlindsEU,		// 15
	BlindsPercentageInverted,	// 16
	Media,				// 17
	Selector,			// 18
	DoorLock,			// 19
	DoorLockInverted,		// 20
	BlindsPercentageWithStop,	// 21
	BlindsPercentageInvertedWithStop, // 22
	END
};

#ifdef INCLUDE_TYPEDEF_CODE
// tiny hack that allows us to keep these definitions together in a single file

const char *Description(const int sType)
{
	static const STR_TABLE_SINGLE Table[] =
	{
		{ device::tswitch::type::OnOff,					"On/Off" },
		{ device::tswitch::type::Doorbell,				"Doorbell" },
		{ device::tswitch::type::Contact,				"Contact" },
		{ device::tswitch::type::Blinds,				"Blinds" },
		{ device::tswitch::type::X10Siren,				"X10 Siren" },
		{ device::tswitch::type::SMOKEDETECTOR,				"Smoke Detector" },
		{ device::tswitch::type::BlindsInverted,			"Blinds Inverted" },
		{ device::tswitch::type::Dimmer,				"Dimmer" },
		{ device::tswitch::type::Motion,				"Motion Sensor" },
		{ device::tswitch::type::PushOn,				"Push On Button" },
		{ device::tswitch::type::PushOff,				"Push Off Button" },
		{ device::tswitch::type::DoorContact,				"Door Contact" },
		{ device::tswitch::type::Dusk,					"Dusk Sensor" },
		{ device::tswitch::type::BlindsPercentage,			"Blinds Percentage" },
		{ device::tswitch::type::VenetianBlindsUS,			"Venetian Blinds US" },
		{ device::tswitch::type::VenetianBlindsEU,			"Venetian Blinds EU" },
		{ device::tswitch::type::BlindsPercentageInverted,		"Blinds Percentage Inverted" },
		{ device::tswitch::type::Media,					"Media Player" },
		{ device::tswitch::type::Selector,				"Selector" },
		{ device::tswitch::type::DoorLock,				"Door Lock" },
		{ device::tswitch::type::DoorLockInverted,			"Door Lock Inverted" },
		{ device::tswitch::type::BlindsPercentageWithStop,		"Blinds + Stop" },
		{ device::tswitch::type::BlindsPercentageInvertedWithStop,	"Blinds Inverted + Stop" },
		{ 0, nullptr }
	};
	return findTableIDSingle1(Table, sType);
}



#else // INCLUDE_TYPEDEF_CODE
const char *Description(const int sType);
#endif // INCLUDE_TYPEDEF_CODE


}; // namespace type
}; // namespace tswitch
}; // namespace device

