#pragma once
#include "typedef/common.hpp"

namespace notification {
namespace type {

enum value {
	TEMPERATURE = 0,	//0
	HUMIDITY,		//1
	RAIN,			//2
	UV,			//3
	WIND,			//4
	USAGE,			//5
	BARO,			//6
	SWITCH_ON,		//7
	AMPERE1,		//8
	AMPERE2,		//9
	AMPERE3,		//10
	ENERGYINSTANT,		//11
	ENERGYTOTAL,		//12
	TODAYENERGY,		//13
	TODAYGAS,		//14
	TODAYCOUNTER,		//15
	SWITCH_OFF,		//16
	PERCENTAGE,		//17
	DEWPOINT,		//18
	RPM,			//19
	SETPOINT,		//20
	VIDEO,			//21
	AUDIO,			//22
	PHOTO,			//23
	PAUSED,			//24
	STOPPED,		//25
	PLAYING,		//26
	VALUE,			//27
	LASTUPDATE,		//28
	END
};

#ifdef INCLUDE_TYPEDEF_CODE
// tiny hack that allows us to keep these definitions together in a single file

const char *Description(const int nType, const unsigned char field)
{
	static const STR_TABLE_SINGLE Table[] =
	{
		{ TEMPERATURE,		"Temperature",	"T" },
		{ HUMIDITY,		"Humidity",	"H" },
		{ RAIN,			"Rain",		"R" },
		{ UV,			"UV",		"U" },
		{ WIND,			"Wind",		"W" },
		{ USAGE,		"Usage",	"M" },
		{ BARO,			"Baro",		"B" },
		{ SWITCH_ON,		"Switch On",	"S" },
		{ AMPERE1,		"Ampere 1",	"1" },
		{ AMPERE2,		"Ampere 2",	"2" },
		{ AMPERE3,		"Ampere 3",	"3" },
		{ ENERGYINSTANT,	"Instant",	"I" },
		{ TODAYENERGY,		"Today",	"E" },
		{ TODAYGAS,		"Today",	"G" },
		{ TODAYCOUNTER,		"Today",	"C" },
		{ SWITCH_OFF,		"Switch Off",	"O" },
		{ PERCENTAGE,		"Percentage",	"P" },
		{ RPM,			"RPM",		"Z" },
		{ DEWPOINT,		"Dew Point",	"D" },
		{ SETPOINT,		"Set Point",	"N" },
		{ VIDEO,		"Play Video",	"V" },
		{ AUDIO,		"Play Audio",	"A" },
		{ PHOTO,		"View Photo",	"X" },
		{ PAUSED,		"Pause Stream",	"Y" },
		{ STOPPED,		"Stop Stream",	"Q" },
		{ PLAYING,		"Play Stream",	"a" },
		{ VALUE,		"Value",	"F" },
		{ LASTUPDATE,		"Last Update",	"J" },
		{ 0, NULL, NULL }
	};
	if (field == 0)
		return findTableIDSingle1(Table, nType);
	else
		return findTableIDSingle2(Table, nType);
}

const char *Label(const int nType)
{
	static const STR_TABLE_SINGLE	Table[] =
	{
		{ TEMPERATURE,		"degrees" },
		{ HUMIDITY,		"%" },
		{ RAIN,			"mm" },
		{ UV,			"UVI" },
		{ WIND,			"m/s" },
		{ USAGE,		"" },
		{ BARO,			"hPa" },
		{ SWITCH_ON,		"" },
		{ AMPERE1,		"Ampere" },
		{ AMPERE2,		"Ampere" },
		{ AMPERE3,		"Ampere" },
		{ ENERGYINSTANT,	"Watt" },
		{ TODAYENERGY,		"kWh" },
		{ TODAYGAS,		"m3" },
		{ TODAYCOUNTER,		"cnt" },
		{ SWITCH_OFF,		"On" },
		{ PERCENTAGE,		"%" },
		{ RPM,			"RPM" },
		{ DEWPOINT,		"degrees" },
		{ SETPOINT,		"degrees" },
		{ VIDEO,		"" },
		{ AUDIO,		"" },
		{ PHOTO,		"" },
		{ PAUSED,		"" },
		{ STOPPED,		"" },
		{ PLAYING,		"" },
		{ VALUE,		"" },
		{ LASTUPDATE,		"minutes" },
		{ 0,NULL,NULL }
	};
	return findTableIDSingle1(Table, nType);
}


#else // INCLUDE_TYPEDEF_CODE
const char *Description(const int nType, const unsigned char field);
const char *Label(const int nType);
#endif // INCLUDE_TYPEDEF_CODE


}; // namespace type
}; // namespace notification


