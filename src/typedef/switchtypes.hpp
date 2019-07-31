#pragma once
#include "typedef/common.hpp"

namespace device {
namespace _switch {
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
	Proximity,			// 21
	END
};

#ifdef INCLUDE_TYPEDEF_CODE
// tiny hack that allows us to keep these definitions together in a single file

const char *Description(const int sType)
{
	static const STR_TABLE_SINGLE Table[] =
	{
		{ device::_switch::type::OnOff,				"On/Off" },
		{ device::_switch::type::Doorbell,			"Doorbell" },
		{ device::_switch::type::Contact,			"Contact" },
		{ device::_switch::type::Blinds,			"Blinds" },
		{ device::_switch::type::X10Siren,			"X10 Siren" },
		{ device::_switch::type::SMOKEDETECTOR,			"Smoke Detector" },
		{ device::_switch::type::BlindsInverted,		"Blinds Inverted" },
		{ device::_switch::type::Dimmer,			"Dimmer" },
		{ device::_switch::type::Motion,			"Motion Sensor" },
		{ device::_switch::type::PushOn,			"Push On Button" },
		{ device::_switch::type::PushOff,			"Push Off Button" },
		{ device::_switch::type::DoorContact,			"Door Contact" },
		{ device::_switch::type::Dusk,				"Dusk Sensor" },
		{ device::_switch::type::BlindsPercentage,		"Blinds Percentage" },
		{ device::_switch::type::VenetianBlindsUS,		"Venetian Blinds US" },
		{ device::_switch::type::VenetianBlindsEU,		"Venetian Blinds EU" },
		{ device::_switch::type::BlindsPercentageInverted,	"Blinds Percentage Inverted" },
		{ device::_switch::type::Media,				"Media Player" },
		{ device::_switch::type::Selector,			"Selector" },
		{ device::_switch::type::DoorLock,			"Door Lock" },
		{ device::_switch::type::DoorLockInverted,		"Door Lock Inverted" },
		{ device::_switch::type::Proximity,			"Proximity Sensor" },
		{ 0, NULL, NULL }
	};
	return findTableIDSingle1(Table, sType);
}



#else // INCLUDE_TYPEDEF_CODE
const char *Description(const int sType);
#endif // INCLUDE_TYPEDEF_CODE


}; // namespace type
}; // namespace _switch
}; // namespace device

