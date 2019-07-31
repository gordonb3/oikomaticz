#pragma once

#include <string>
#include "typedef/hardwaretypes.hpp"
#include "typedef/metertypes.hpp"
#include "typedef/switchtypes.hpp"
#include "typedef/timertypes.hpp"
#include "typedef/notificationtypes.hpp"
#include "typedef/mediaplayerstates.hpp"


#define sTypeTH_LC_TC 0xA0   //La Cross Temp_Hum combined
#define sTypeTEMP_SYSTEM 0xA0  //Internal sensor

const char *RFX_Type_Desc(const unsigned char i, const unsigned char snum);
const char *RFX_Type_SubType_Desc(const unsigned char dType, const unsigned char sType);
unsigned char Get_Humidity_Level(const unsigned char hlevel);
const char *RFX_Humidity_Status_Desc(const unsigned char status);
const char *RFX_Forecast_Desc(const unsigned char Forecast);
const char *RFX_WSForecast_Desc(const unsigned char Forecast);
const char *BMP_Forecast_Desc(const unsigned char Forecast);
const char *Security_Status_Desc(const unsigned char status);
const char *Get_Moisture_Desc(const int moisture);
const char *Get_Alert_Desc(const int level);
const char *ZWave_Clock_Days(const unsigned char Day);
extern const char *ZWave_Thermostat_Fan_Modes[];
int Lookup_ZWave_Thermostat_Modes(const std::vector<std::string> &Modes, const std::string &sMode);
int Lookup_ZWave_Thermostat_Fan_Modes(const std::string &sMode);

void GetLightStatus(
	const unsigned char dType,
	const unsigned char dSubType,
	const device::tswitch::type::value switchtype,
	const unsigned char nValue,
	const std::string &sValue,
	std::string &lstatus,
	int &llevel,
	bool &bHaveDimmer,
	int &maxDimLevel,
	bool &bHaveGroupCmd);

int  GetSelectorSwitchLevel(const std::map<std::string, std::string> & options, const std::string & levelName);
std::string GetSelectorSwitchLevelAction(const std::map<std::string, std::string> & options, const int level);
void GetSelectorSwitchStatuses(const std::map<std::string, std::string> & options, std::map<std::string, std::string> & statuses);

bool GetLightCommand(
	const unsigned char dType,
	const unsigned char dSubType,
	const device::tswitch::type::value switchtype,
	std::string switchcmd,
	unsigned char &cmd,
	const std::map<std::string, std::string> & options
);

bool IsLightSwitchOn(const std::string &lstatus);

bool IsSerialDevice(const hardware::type::value htype);
bool IsNetworkDevice(const hardware::type::value htype);
void ConvertToGeneralSwitchType(std::string &devid, int &dtype, int &subtype);
