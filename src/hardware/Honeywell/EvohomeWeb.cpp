/**
 * Json client for UK/EMEA Evohome API
 *
 *  Adapted for integration with Domoticz
 *
 *  Copyright 2017 - gordonb3 https://github.com/gordonb3/evohomeclient
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 *  @license GPL-3.0+ <https://github.com/gordonb3/evohomeclient/blob/master/LICENSE>
 */
#include "stdafx.h"
#include "EvohomeWeb.h"
#include "main/Helper.h"
#include "main/localtime_r.h"
#include "main/Logger.h"
#include "main/SQLHelper.h"
#include "main/mainworker.h"
#include "hardware/hardwaretypes.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "evohomeclient/src/evohomeclient/evohomeclient.hpp"
#include "evohomeclient/src/evohomeclient2/evohomeclient2.hpp"
#include "evohomeclient/src/evohome/jsoncppbridge.hpp"

#define LOGONFAILTRESHOLD 3
#define MINPOLINTERVAL 10
#define MAXPOLINTERVAL 3600
#define HTTPTIMEOUT 30

#ifdef _WIN32
#define gmtime_r(timep, result) gmtime_s(result, timep)
#endif


namespace evohome {
  namespace WebAPI {
    EvohomeClient* v1;
    EvohomeClient2* v2;

    evohome::device::temperatureControlSystem* tcs2;
  };
};



const uint8_t CEvohomeWeb::m_dczToEvoWebAPIMode[7] = { 0,2,3,4,6,1,5 };
const std::string CEvohomeWeb::weekdays[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };


CEvohomeWeb::CEvohomeWeb(const int ID, const std::string &Username, const std::string &Password, const unsigned int refreshrate, const int UseFlags, const unsigned int installation) :
	m_username(Username),
	m_password(Password),
	m_refreshrate(refreshrate)
{
	m_HwdID = ID;
	m_bSkipReceiveCheck = true;

	m_locationIdx = (installation >> 12) & 15;
	m_gatewayIdx = (installation >> 8) & 15;
	m_systemIdx = (installation >> 4) & 15;


	/* Use Flags
	 *
	 * 0x1 = m_updatedev (let Honeywell server control the device name)
	 * 0x2 = m_showschedule (show next scheduled switch point as `until` time)
	 * 0x4 = m_showlocation (prefix device name with location)
	 * 0x8 = m_showhdtemps (show better precision temps from 'old' US Evohome API)
	 * 0x10 = m_precision (0 = 1 decimal, 1 = 2 decimals)
	 *
	 */

	m_updatedev = ((UseFlags & 1) == 0); // reverted value: default = true
	m_showschedule = ((UseFlags & 2) > 0);
	m_showlocation = ((UseFlags & 4) > 0);
	m_showhdtemps = ((UseFlags & 8) > 0);
	m_hdprecision = ((UseFlags & 16) > 0) ? 2 : 1;

	if (m_refreshrate < 10)
		m_refreshrate = 60;

	m_awaysetpoint = 0; // we'll fetch this from the controller device status later
	m_wdayoff = 6; // saturday
}


CEvohomeWeb::~CEvohomeWeb(void)
{
	m_bIsStarted = false;
}


void CEvohomeWeb::Init()
{
	m_loggedon = false;
	m_logonfailures = 0;
	m_szlocationName = "";

	m_j_stat.clear();

	if (m_showhdtemps)
		evohome::WebAPI::v1 = new EvohomeClient();
	evohome::WebAPI::v2 = new EvohomeClient2();
}



int CEvohomeWeb::GetLastV2ResponseCode()
{
	std::string szResponse = evohome::WebAPI::v2->get_last_response();

	Json::Value jResponse;
	if (!evohome::parse_json_string(szResponse, jResponse))
		return -1;

	return atoi(jResponse["code"].asString().c_str());
}


bool CEvohomeWeb::StartSession()
{
	if (m_loggedon)
	{
		if (!evohome::WebAPI::v2->is_session_valid() && !evohome::WebAPI::v2->renew_login())
		{
			_log.Log(LOG_ERROR, "(%s) login failed with message: %s", m_Name.c_str(), evohome::WebAPI::v2->get_last_error().c_str());
			int returnCode = GetLastV2ResponseCode();
			if (returnCode >= 400)
				m_loggedon = false;
		}
	}

	if (!m_loggedon)
	{
		_log.Log(LOG_STATUS, "(%s) connect to Evohome server", m_Name.c_str());
		if (!evohome::WebAPI::v2->login(m_username, m_password))
		{
			_log.Log(LOG_ERROR, "(%s) login failed with message: %s", m_Name.c_str(), evohome::WebAPI::v2->get_last_error().c_str());
			m_logonfailures++;
			if (m_logonfailures == LOGONFAILTRESHOLD)
				_log.Log(LOG_STATUS, "(%s) logon fail treshold reached - trottling", m_Name.c_str());
			if ((m_logonfailures * m_refreshrate) > MAXPOLINTERVAL)
				m_logonfailures--;
			return false;
		}
		m_loggedon = true;
	}

	m_logonfailures = 0;

	// (re)initialize Evohome installation info
	m_zones[0] = 0;
	if (!evohome::WebAPI::v2->full_installation())
	{
		_log.Log(LOG_ERROR, "(%s) failed to retrieve installation info from server", m_Name.c_str());
		return false;
	}

	evohome::WebAPI::tcs2 = NULL;
	if (
		(evohome::WebAPI::v2->m_vLocations.size() > m_locationIdx) &&
		(evohome::WebAPI::v2->m_vLocations[m_locationIdx].gateways.size() > m_gatewayIdx) &&
		(evohome::WebAPI::v2->m_vLocations[m_locationIdx].gateways[m_gatewayIdx].temperatureControlSystems.size() > m_systemIdx)
		)
	{
		evohome::WebAPI::tcs2 = &evohome::WebAPI::v2->m_vLocations[m_locationIdx].gateways[m_gatewayIdx].temperatureControlSystems[m_systemIdx];
		m_szlocationName = evohome::WebAPI::v2->get_location_name(m_locationIdx);
	}
	else
	{
		_log.Log(LOG_ERROR, "(%s) installation at [%d,%d,%d] does not exist - verify your settings", m_Name.c_str(), m_locationIdx, m_gatewayIdx, m_systemIdx);
		return false;
	}

	if (m_awaysetpoint == 0) // first run - try to get our Away setpoint value from the controller device status
	{
		std::vector<std::vector<std::string> > result;
		result = m_sql.safe_query("SELECT Extra FROM Hardware WHERE ID=%d", this->m_HwdID);
		if (!result.empty()) // adding hardware
		{
			std::vector<std::string> splitresults;
			StringSplit(result[0][0], ";", splitresults);
			if (splitresults.size()>0)
				m_awaysetpoint = strtod(splitresults[0].c_str(), NULL);
			if (splitresults.size()>1)
				m_wdayoff = atoi(splitresults[1].c_str()) % 7;
		}
		if (m_awaysetpoint == 0)
			m_awaysetpoint = 15; // use default 'Away' setpoint value
	}

	return true;
}


bool CEvohomeWeb::StartHardware()
{
	RequestStart();

	if (m_username.empty() || m_password.empty())
		return false;
	Init();
	m_thread = std::make_shared<std::thread>(&CEvohomeWeb::Do_Work, this);
	SetThreadNameInt(m_thread->native_handle());
	if (!m_thread)
		return false;
	m_bIsStarted = true;
	sOnConnected(this);
	return true;
}


bool CEvohomeWeb::StopHardware()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	m_bIsStarted = false;
	return true;
}


void CEvohomeWeb::Do_Work()
{
	int sec_counter = m_refreshrate - MINPOLINTERVAL;
	int pollcounter = LOGONFAILTRESHOLD;
	_log.Log(LOG_STATUS, "(%s) Worker started...", m_Name.c_str());
	m_lastconnect=0;
	while (!IsStopRequested(1000))
	{
		sec_counter++;
		m_lastconnect++;
		if (sec_counter % 10 == 0)
			m_LastHeartbeat = mytime(NULL);

		if ((sec_counter % m_refreshrate == 0) && (pollcounter++ > m_logonfailures) && (m_lastconnect >= MINPOLINTERVAL))
		{
			GetStatus();
			pollcounter = LOGONFAILTRESHOLD;
			m_lastconnect=0;
		}
	}

	_log.Log(LOG_STATUS, "(%s) Worker stopped...", m_Name.c_str());
}


bool CEvohomeWeb::WriteToHardware(const char *pdata, const unsigned char length)
{
	if (!pdata)
		return false;
	if (!m_loggedon && !StartSession())
		return false;
	if (evohome::WebAPI::v2->m_vLocations.size() < (size_t)m_locationIdx)
		return false;
	if (evohome::WebAPI::v2->m_vLocations[m_locationIdx].jStatus.isNull() && !GetStatus())
		return false;

	m_lastconnect=0;
	switch (pdata[1])
	{
	case pTypeEvohome:
		if (length < sizeof(_tEVOHOME1))
			return false;
		return SetSystemMode(((_tEVOHOME1*)pdata)->status);
		break;
	case pTypeEvohomeZone:
		if (length < sizeof(_tEVOHOME2))
			return false;
		return SetSetpoint(pdata);
		break;
	case pTypeEvohomeWater:
		if (length < sizeof(_tEVOHOME2))
			return false;
		return SetDHWState(pdata);
		break;
	}
	return false; // bad command
}


bool CEvohomeWeb::GetStatus()
{
	if (!evohome::WebAPI::v2->is_session_valid() && !StartSession())
		return false;
	if (!evohome::WebAPI::v2->get_status(evohome::WebAPI::v2->m_vLocations[m_locationIdx].szLocationId))
	{
		int returnCode = GetLastV2ResponseCode();
		if (returnCode >= 400)
			m_loggedon = false;
		return false;
	}
	_log.Log(LOG_NORM, "(%s) fetch data from server", m_Name.c_str());

	// system status
	DecodeControllerMode(evohome::WebAPI::tcs2);

	if (m_showhdtemps)
	{
		if (!evohome::WebAPI::v1->is_session_valid() && !evohome::WebAPI::v1->login(m_username, m_password))
		{
			_log.Log(LOG_ERROR, "(%s) failed login to v1 API", m_Name.c_str());
		}
		else if (!evohome::WebAPI::v1->full_installation())
		{
			_log.Log(LOG_ERROR, "(%s) error fetching v1 data from server", m_Name.c_str());
		}
	}

	// cycle all zones for status
	int numZones = static_cast<int>(evohome::WebAPI::tcs2->zones.size());
	for (int i = 0; i < numZones; ++i)
		DecodeZone(&evohome::WebAPI::tcs2->zones[i]);

	// hot water status
	if (evohome::WebAPI::v2->has_dhw(evohome::WebAPI::tcs2))
		DecodeDHWState(evohome::WebAPI::tcs2);

	return true;
}


bool CEvohomeWeb::SetSystemMode(uint8_t sysmode)
{
	std::string sznewmode = GetWebAPIModeName(sysmode);
	if (evohome::WebAPI::v2->set_system_mode(evohome::WebAPI::tcs2->szSystemId, (int)(m_dczToEvoWebAPIMode[sysmode])))
	{
		_log.Log(LOG_NORM, "(%s) changed system status to %s", m_Name.c_str(), GetControllerModeName(sysmode));

		int numZones = static_cast<int>(evohome::WebAPI::tcs2->zones.size());
		if (sznewmode == "HeatingOff")
		{
			// cycle my zones to reflect the HeatingOff mode
			for (int i = 0; i < numZones; ++i)
			{
				evohome::device::zone* hz = &evohome::WebAPI::tcs2->zones[i];
				std::string szId, sztemperature;
				szId = hz->szZoneId;
				if (m_showhdtemps)
					sztemperature = evohome::WebAPI::v1->get_zone_temperature(szId, m_hdprecision);
				else if (hz->jStatus != NULL)
					sztemperature = evohome::WebAPI::v2->get_zone_temperature(hz);

				if ((sztemperature.empty()) || (sztemperature == "128"))
				{
					sztemperature = "-";
					sznewmode = "Offline";
				}
				unsigned long evoID = atol(szId.c_str());
				std::stringstream ssUpdateStat;
				ssUpdateStat << sztemperature << ";5;" << sznewmode;
				std::string sdevname;
				m_sql.UpdateValue(this->m_HwdID, szId.c_str(), GetUnit_by_ID(evoID), pTypeEvohomeZone, sTypeEvohomeZone, 10, 255, 0, ssUpdateStat.str().c_str(), sdevname);
			}
			return true;
		}

		// cycle my zones to restore scheduled temps
		for (int i = 0; i < numZones; ++i)
		{
			evohome::device::zone* hz = &evohome::WebAPI::tcs2->zones[i];
			std::string zonemode = "";
			if (hz->jStatus == NULL) // don't touch invalid zone - it should already show as 'Offline'
				continue;
			if (hz->jStatus->isMember("heatSetpointStatus"))
				zonemode = (*hz->jStatus)["heatSetpointStatus"]["setpointMode"].asString();
			if ((zonemode.size() > 9) && (zonemode.substr(9) == "Override")) // don't touch zone if it is in Override mode
				continue;

			std::string sztemperature, szsetpoint;
			std::string szuntil = "";
			double setpoint = 0;

			/*  there is no strict definition for modes Away, DayOff and Custom so we'll have to wait
			 *  for the next update to get the correct values. But we can make educated guesses
			 */

			// Away unconditionally sets all zones to a preset temperature, even if Normal mode is lower
			if (sznewmode == "Away")
				setpoint = m_awaysetpoint;
			else
			{
				if ((!hz->jSchedule.isNull()) || evohome::WebAPI::v2->get_zone_schedule(hz->szZoneId))
				{
					szuntil = evohome::WebAPI::v2->get_next_switchpoint(hz, szsetpoint, false);
					setpoint = strtod(szsetpoint.c_str(), NULL);
				}

				// Eco lowers the setpoint of all zones by 3 degrees, but resets a zone mode to Normal setting
				// if the resulting setpoint is below the Away setpoint
				if ((sznewmode == "AutoWithEco") && (setpoint >= (m_awaysetpoint + 3)))
					setpoint -= 3;
			}

			if (m_showhdtemps)
				sztemperature = evohome::WebAPI::v1->get_zone_temperature(hz->szZoneId, m_hdprecision);
			else if (hz->jStatus != NULL)
				sztemperature = evohome::WebAPI::v2->get_zone_temperature(hz);

			if ((sztemperature.empty()) || (sztemperature == "128"))
			{
				sztemperature = "-";
				sznewmode = "Offline";
			}

			unsigned long evoID = atol(hz->szZoneId.c_str());
			std::stringstream ssUpdateStat;
			if (setpoint < 5) // there was an error - no schedule?
				ssUpdateStat << sztemperature << ";5;Unknown";
			else
			{
				ssUpdateStat << sztemperature << ";" << setpoint << ";" << sznewmode;
				if ((m_showschedule) && !(szuntil.empty()) && (sznewmode != "Custom"))
					ssUpdateStat << ";" << szuntil;
			}
			std::string sdevname;
			m_sql.UpdateValue(this->m_HwdID, hz->szZoneId.c_str(), GetUnit_by_ID(evoID), pTypeEvohomeZone, sTypeEvohomeZone, 10, 255, 0, ssUpdateStat.str().c_str(), sdevname);
		}
		return true;
	}
	_log.Log(LOG_ERROR, "(%s) error changing system status", m_Name.c_str());
	m_loggedon = false;
	return false;
}


bool CEvohomeWeb::SetSetpoint(const char *pdata)
{
	_tEVOHOME2 *pEvo = (_tEVOHOME2*)pdata;
	std::string zoneId(std::to_string((int)RFX_GETID3(pEvo->id1, pEvo->id2, pEvo->id3)));

	evohome::device::zone* hz = evohome::WebAPI::v2->get_zone_by_ID(zoneId);
	if (hz == NULL) // zone number not known by installation (manually added?)
	{
		_log.Log(LOG_ERROR, "(%s) attempt to change setpoint on unknown zone", m_Name.c_str());
		return false;
	}

	if ((pEvo->mode) == 0) // cancel override
	{
		if (!evohome::WebAPI::v2->cancel_temperature_override(zoneId))
			return false;

		std::string szsetpoint;
		std::string szuntil = "";
		if ((!hz->jSchedule.isNull()) || evohome::WebAPI::v2->get_zone_schedule(hz->szZoneId))
		{
			szuntil = evohome::WebAPI::v2->get_next_switchpoint(hz, szsetpoint, false);
			pEvo->temperature = (int16_t)(strtod(szsetpoint.c_str(), NULL) * 100);
		}

		if ((m_showschedule) && (!szuntil.empty()))
		{
			pEvo->year = (uint16_t)(atoi(szuntil.substr(0, 4).c_str()));
			pEvo->month = (uint8_t)(atoi(szuntil.substr(5, 2).c_str()));
			pEvo->day = (uint8_t)(atoi(szuntil.substr(8, 2).c_str()));
			pEvo->hrs = (uint8_t)(atoi(szuntil.substr(11, 2).c_str()));
			pEvo->mins = (uint8_t)(atoi(szuntil.substr(14, 2).c_str()));
		}
		else
			pEvo->year = 0;
		return true;
	}

	int temperature_int = (int)pEvo->temperature / 100;
	int temperature_frac = (int)pEvo->temperature % 100;
	std::stringstream s_setpoint;
	s_setpoint << temperature_int << "." << temperature_frac;

	if ((pEvo->mode) == 1) // permanent override
	{
		return evohome::WebAPI::v2->set_temperature(zoneId, s_setpoint.str(), "");
	}
	if ((pEvo->mode) == 2) // temporary override
	{
		std::string szISODate(CEvohomeDateTime::GetISODate(pEvo));
		return evohome::WebAPI::v2->set_temperature(zoneId, s_setpoint.str(), szISODate);
	}
	return false;
}


bool CEvohomeWeb::SetDHWState(const char *pdata)
{
	if (!evohome::WebAPI::v2->has_dhw(evohome::WebAPI::tcs2)) // Installation has no Hot Water device
	{
		_log.Log(LOG_ERROR, "(%s) attempt to set state on non existing Hot Water device", m_Name.c_str());
		return false;
	}

	_tEVOHOME2 *pEvo = (_tEVOHOME2*)pdata;

	std::string dhwId(std::to_string((int)RFX_GETID3(pEvo->id1, pEvo->id2, pEvo->id3)));

	std::string DHWstate = (pEvo->temperature == 0) ? "off" : "on";

	if ((pEvo->mode) == 0) // cancel override (web front end does not appear to support this?)
	{
		DHWstate = "auto";
	}
	if ((pEvo->mode) <= 1) // permanent override
	{
		return evohome::WebAPI::v2->set_dhw_mode(dhwId, DHWstate, "");
	}
	if ((pEvo->mode) == 2) // temporary override
	{
		std::string szISODate(CEvohomeDateTime::GetISODate(pEvo));
		return evohome::WebAPI::v2->set_dhw_mode(dhwId, DHWstate, szISODate);
	}
	return false;
}


void CEvohomeWeb::DecodeControllerMode(evohome::device::temperatureControlSystem* tcs)
{
	unsigned long ID = (unsigned long)(strtod(tcs->szSystemId.c_str(), NULL));
	std::string szsystemMode, szmodelType;
	uint8_t sysmode = 0;

	if (tcs->jStatus == NULL)
		szsystemMode = "Unknown";
	else
		szsystemMode = (*tcs->jStatus)["systemModeStatus"]["mode"].asString();
	while (sysmode < 7 && strcmp(szsystemMode.c_str(), m_szWebAPIMode[sysmode]) != 0)
		sysmode++;

	_tEVOHOME1 tsen;
	memset(&tsen, 0, sizeof(_tEVOHOME1));
	tsen.len = sizeof(_tEVOHOME1) - 1;
	tsen.type = pTypeEvohome;
	tsen.subtype = sTypeEvohome;
	RFX_SETID3(ID, tsen.id1, tsen.id2, tsen.id3);
	tsen.mode = 0; // web API does not support temp override of controller mode
	tsen.status = sysmode;
	sDecodeRXMessage(this, (const unsigned char *)&tsen, "Controller mode", -1);

	if (GetControllerName().empty() || m_updatedev)
	{
		szmodelType = (*tcs->jInstallationInfo)["modelType"].asString();
		SetControllerName(szmodelType);
		if (szmodelType.empty())
			return;

		std::string devname;
		if (m_showlocation)
			devname = m_szlocationName + ": " + szmodelType;
		else
			devname = szmodelType;

		std::vector<std::vector<std::string> > result;
		result = m_sql.safe_query("SELECT HardwareID, DeviceID, Name, StrParam1 FROM DeviceStatus WHERE HardwareID=%d AND DeviceID='%s'", this->m_HwdID, tcs->szSystemId.c_str());
		if (!result.empty() && ((result[0][2] != devname) || (!result[0][3].empty())))
		{
			// also change lastupdate time to allow the web frontend to pick up the change
			time_t now = mytime(NULL);
			struct tm ltime;
			localtime_r(&now, &ltime);
			// also wipe StrParam1 - we do not also want to call the old (python) script when changing system mode
			m_sql.safe_query("UPDATE DeviceStatus SET Name='%q', LastUpdate='%04d-%02d-%02d %02d:%02d:%02d', StrParam1='' WHERE HardwareID=%d AND DeviceID='%s'", devname.c_str(), ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec, this->m_HwdID, tcs->szSystemId.c_str());
		}
	}
}


void CEvohomeWeb::DecodeZone(evohome::device::zone* hz)
{
	// no sense in using REVOBUF EVOHOME2 to send this to mainworker as this requires breaking up our data
	// only for mainworker to reassemble it.

	std::string szsetpoint, szmode, szuntil;
	std::string sztemperature= "<null>";
	std::stringstream ssUpdateStat;

	std::string szId = hz->szZoneId;

	if (hz->jStatus == NULL)
	{
		sztemperature = "-";
		szsetpoint = "-";
		szmode = "Offline";
	}
	else
	{
		if (m_showhdtemps)
			sztemperature = evohome::WebAPI::v1->get_zone_temperature(szId, m_hdprecision);

		if (!m_showhdtemps || (sztemperature == "<null>"))
			sztemperature = evohome::WebAPI::v2->get_zone_temperature(hz);

		if ((sztemperature.empty()) || (sztemperature == "128") || (sztemperature == "<null>"))
		{
			sztemperature = "-";
			szsetpoint = "-";
			szmode = "Offline";
		}
		else
		{
			szsetpoint = evohome::WebAPI::v2->get_zone_setpoint(hz);
			szmode = evohome::WebAPI::v2->get_zone_mode(hz);

			if (szmode == "TemporaryOverride")
				szuntil = evohome::WebAPI::v2->get_zone_mode_until(hz);
		}
	}

std::cout << "Decode zone: " << szId << " => T: " << sztemperature << " , S: " << szsetpoint << " , M: " << szmode << "\n";

	unsigned long evoID = atol(szId.c_str());
	std::string szsysmode;
	if (evohome::WebAPI::tcs2->jStatus == NULL)
		szsysmode = "Unknown";
	else
		szsysmode = (*evohome::WebAPI::tcs2->jStatus)["systemModeStatus"]["mode"].asString();
	if ((szsysmode == "Away") && (szmode == "FollowSchedule"))
	{
		double new_awaysetpoint = strtod(szsetpoint.c_str(), NULL);
		if (m_awaysetpoint != new_awaysetpoint)
		{
			m_awaysetpoint = new_awaysetpoint;
			m_sql.safe_query("UPDATE Hardware SET Extra='%0.2f;%d' WHERE ID=%d", m_awaysetpoint, m_wdayoff, this->m_HwdID);
			_log.Log(LOG_STATUS, "(%s) change Away setpoint to '%0.2f' because of non matching setpoint (%s)", m_Name.c_str(), m_awaysetpoint, (*hz->jInstallationInfo)["name"].asString().c_str());
		}
		ssUpdateStat << sztemperature << ";" << szsetpoint << ";" << szsysmode;
	}
	else if ((szsysmode == "HeatingOff") && (szmode != "Offline"))
		ssUpdateStat << sztemperature << ";" << szsetpoint << ";" << szsysmode;
	else
	{
		ssUpdateStat << sztemperature << ";" << szsetpoint << ";";
		if (szmode != "FollowSchedule")
			ssUpdateStat << szmode;
		else
		{
			ssUpdateStat << szsysmode;
			if (m_showschedule && szuntil.empty())
			{
				if (szsysmode != "Custom") // can't use schedule to find next switchpoint
					szuntil = evohome::WebAPI::v2->get_next_switchpoint(hz, false);
			}
			if (szsysmode == "DayOff")
			{
				if (szuntil.empty()) // make sure our schedule is populated
					evohome::WebAPI::v2->get_next_switchpoint(hz, true);
				if (szsetpoint != hz->jSchedule["currentSetpoint"].asString())
				{
					bool found = false;
					m_wdayoff--;
					for (uint8_t i = 0; (i < 7) && !found; i++)
					{
						hz->jSchedule["nextSwitchpoint"] = ""; // force a recalculation
						m_wdayoff++;
						if (m_wdayoff>6)
							m_wdayoff -= 7;
						evohome::WebAPI::v2->get_next_switchpoint(hz, true);
						found = (szsetpoint == hz->jSchedule["currentSetpoint"].asString());
					}
					if (found)
					{
						m_sql.safe_query("UPDATE Hardware SET Extra='%0.2f;%d' WHERE ID=%d", m_awaysetpoint, m_wdayoff, this->m_HwdID);

						_log.Log(LOG_STATUS, "(%s) change Day Off schedule reference to '%s' because of non matching setpoint (%s)", m_Name.c_str(), weekdays[m_wdayoff].c_str(), (*hz->jInstallationInfo)["name"].asString().c_str());
					}
					if (m_showschedule)
						szuntil = evohome::WebAPI::v2->get_next_switchpoint(hz, false);
				}
			}
		}
		if (!szuntil.empty())
			ssUpdateStat << ";" << szuntil;
	}

	std::string sdevname;
	uint64_t DevRowIdx = m_sql.UpdateValue(this->m_HwdID, szId.c_str(), GetUnit_by_ID(evoID), pTypeEvohomeZone, sTypeEvohomeZone, 10, 255, 0, ssUpdateStat.str().c_str(), sdevname);

	if (m_updatedev && (DevRowIdx != -1))
	{
		std::stringstream ssnewname;
		if (m_showlocation)
			ssnewname << m_szlocationName << ": ";
		ssnewname << (*hz->jInstallationInfo)["name"].asString();

		if (sdevname != ssnewname.str())
		{
			// also wipe StrParam1 - we do not want a double action from the old (python) script when changing the setpoint
			m_sql.safe_query("UPDATE DeviceStatus SET Name='%q', StrParam1='' WHERE (ID == %" PRIu64 ")", ssnewname.str().c_str(), DevRowIdx);
			if (sdevname.find("zone ") != std::string::npos)
				_log.Log(LOG_STATUS, "(%s) register new zone '%s'", m_Name.c_str(), ssnewname.str().c_str());
		}
	}

	// Notify MQTT and various push mechanisms
	m_mainworker.sOnDeviceReceived(this->m_HwdID, DevRowIdx, (*hz->jInstallationInfo)["name"].asString(), NULL);
}


void CEvohomeWeb::DecodeDHWState(evohome::device::temperatureControlSystem* tcs)
{
	if (!evohome::WebAPI::v2->has_dhw(tcs))
		return;

	// Hot Water is essentially just another zone
	evohome::device::zone* hz = &(tcs->dhw[0]);

	if (!(*hz->jStatus).isMember("temperatureStatus") || !(*hz->jStatus).isMember("stateStatus"))
		return;

	std::string szId, szmode;
	std::string szuntil = "";
	std::stringstream ssUpdateStat;

	szId = (*hz->jStatus)["dhwId"].asString();
	ssUpdateStat << (*hz->jStatus)["temperatureStatus"]["temperature"].asString() << ";";
	ssUpdateStat << (*hz->jStatus)["stateStatus"]["state"].asString() << ";";
	szmode = (*hz->jStatus)["stateStatus"]["mode"].asString();
	if (szmode == "FollowSchedule")
		ssUpdateStat << "Auto";
	else
		ssUpdateStat << szmode;
	if (szmode == "TemporaryOverride")
		szuntil = (*hz->jStatus)["stateStatus"]["until"].asString();

	if (m_updatedev) // create/update and return the first free unit
	{
		std::vector<std::vector<std::string> > result;
		result = m_sql.safe_query(
			"SELECT ID,DeviceID,Name FROM DeviceStatus WHERE (HardwareID==%d) AND (Type==%d) ORDER BY Unit",
			this->m_HwdID, pTypeEvohomeWater);
		std::string ndevname;
		if (m_showlocation)
			ndevname = m_szlocationName + ": Hot Water";
		else
			ndevname = "Hot Water";

		if (result.empty())
		{
			// create device
			std::string sdevname;
			uint64_t DevRowIdx = m_sql.UpdateValue(this->m_HwdID, szId.c_str(), 1, pTypeEvohomeWater, sTypeEvohomeWater, 10, 255, 50, "0.0;Off;Auto", sdevname);
			m_sql.safe_query("UPDATE DeviceStatus SET Name='%q' WHERE (ID == %" PRIu64 ")", ndevname.c_str(), DevRowIdx);
		}
		else if ((result[0][1] != szId) || (result[0][2] != ndevname))
		{
			// also wipe StrParam1 - we do not want a double action from the old (python) script when changing the setpoint
			m_sql.safe_query("UPDATE DeviceStatus SET DeviceID='%q', Name='%q', StrParam1='' WHERE (ID == %" PRIu64 ")", szId.c_str(), ndevname.c_str(), std::stoull(result[0][0]));
		}
	}

	if (m_showschedule && szuntil.empty())
	{
		std::string szsysmode = (*tcs->jStatus)["systemModeStatus"]["mode"].asString();
		if (szsysmode != "Custom") // can't use schedule to find next switchpoint
		{
			if (!(hz->jSchedule.isNull()) || evohome::WebAPI::v2->get_dhw_schedule(hz->szZoneId))
				szuntil = evohome::WebAPI::v2->get_next_switchpoint(hz, false);
		}
	}
	if (!szuntil.empty())
		ssUpdateStat << ";" << szuntil;

	std::string sdevname;
	uint64_t DevRowIdx = m_sql.UpdateValue(this->m_HwdID, szId.c_str(), 1, pTypeEvohomeWater, sTypeEvohomeWater, 10, 255, 50, ssUpdateStat.str().c_str(), sdevname);

	// Notify MQTT and various push mechanisms
	m_mainworker.sOnDeviceReceived(this->m_HwdID, DevRowIdx, "Hot Water", NULL);
}


/*
 * Code for serial and python scripts appear to assume that zones are always returned in the same order
 * I'm not sure that is really true, so I'll use a map to match the evohome ID and our zone number.
 */
uint8_t CEvohomeWeb::GetUnit_by_ID(unsigned long evoID)
{
	size_t row;
	if (m_zones[0] == 0) // first run - construct
	{
		std::vector<std::vector<std::string> > result;
		result = m_sql.safe_query(
			"SELECT Unit,DeviceID FROM DeviceStatus WHERE (HardwareID==%d) AND (Type==%d) ORDER BY Unit",
			this->m_HwdID, pTypeEvohomeZone);
		for (row = 1; row <= m_nMaxZones; row++)
			m_zones[row] = 0;
		for (row = 0; row < result.size(); row++)
		{
			int unit = atoi(result[row][0].c_str());
			m_zones[unit] = atol(result[row][1].c_str());
			if (m_zones[unit] == (unsigned long)(unit + 92000)) // mark manually added, unlinked zone as free
				m_zones[unit] = 0;
		}
		m_zones[0] = 1;
	}
	unsigned char unit = 0;
	for (row = 1; row <= m_nMaxZones; row++)
	{
		unit++;
		if (m_zones[row] == evoID)
			return (uint8_t)(unit);
	}
	if (m_updatedev) // create/update and return the first free unit
	{
		unit = 0;
		for (row = 1; row <= m_nMaxZones; row++)
		{
			unit++;
			if (m_zones[row] == 0)
			{
				std::string sdevname;
				unsigned long nid = 92000 + row;
				char ID[40];
				sprintf(ID, "%lu", nid);
				uint64_t DevRowIdx = m_sql.UpdateValue(this->m_HwdID, ID, unit, pTypeEvohomeZone, sTypeEvohomeZone, 10, 255, 0, "0.0;0.0;Auto", sdevname);
				if (DevRowIdx == -1)
					return (uint8_t)-1;
				char devname[8];
				sprintf(devname, "zone %d", (int)row);
				sprintf(ID, "%lu", evoID);
				m_sql.safe_query("UPDATE DeviceStatus SET Name='%q',DeviceID='%q' WHERE (ID == %" PRIu64 ")", devname, ID, DevRowIdx);
				m_zones[row] = evoID;
				return (uint8_t)(unit);
			}
		}
		_log.Log(LOG_ERROR, "(%s) cannot add new zone because you have no free zones left", m_Name.c_str());
	}
	return (uint8_t)-1;
}




