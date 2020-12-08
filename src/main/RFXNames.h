#pragma once

#include <string>
#include "typedef/hardwaretypes.hpp"
#include "typedef/metertypes.hpp"
#include "typedef/switchtypes.hpp"
#include "typedef/timertypes.hpp"
#include "typedef/notificationtypes.hpp"
#include "typedef/mediaplayerstates.hpp"


const char *RFX_Type_Desc(unsigned char i, unsigned char snum);
const char *RFX_Type_SubType_Desc(unsigned char dType, unsigned char sType);
unsigned char Get_Humidity_Level(unsigned char hlevel);
const char *RFX_Humidity_Status_Desc(unsigned char status);
const char *RFX_Forecast_Desc(unsigned char Forecast);
const char *RFX_WSForecast_Desc(unsigned char Forecast);
const char *BMP_Forecast_Desc(unsigned char Forecast);
const char *Security_Status_Desc(unsigned char status);
const char *Get_Moisture_Desc(int moisture);
const char *Get_Alert_Desc(int level);
const char *ZWave_Clock_Days(unsigned char Day);
extern const char *ZWave_Thermostat_Fan_Modes[];
int Lookup_ZWave_Thermostat_Modes(const std::vector<std::string> &Modes, const std::string &sMode);
int Lookup_ZWave_Thermostat_Fan_Modes(const std::string &sMode);

void GetLightStatus(unsigned char dType, unsigned char dSubType, device::tswitch::type::value switchtype, unsigned char nValue, const std::string &sValue, std::string &lstatus, int &llevel, bool &bHaveDimmer,
		    int &maxDimLevel, bool &bHaveGroupCmd);

int  GetSelectorSwitchLevel(const std::map<std::string, std::string> & options, const std::string & levelName);
std::string GetSelectorSwitchLevelAction(const std::map<std::string, std::string> &options, int level);
void GetSelectorSwitchStatuses(const std::map<std::string, std::string> & options, std::map<std::string, std::string> & statuses);

bool GetLightCommand(unsigned char dType, unsigned char dSubType, device::tswitch::type::value switchtype, std::string switchcmd, unsigned char &cmd, const std::map<std::string, std::string> &options);

bool IsLightSwitchOn(const std::string &lstatus);

bool IsSerialDevice(hardware::type::value htype);
bool IsNetworkDevice(hardware::type::value htype);
void ConvertToGeneralSwitchType(std::string &devid, int &dtype, int &subtype);
