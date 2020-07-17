/**
 * client interface for UK/EMEA Evohome API
 *
 *  Copyright 2017-2020 - gordonb3 https://github.com/gordonb3/evohomeclient
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
#include "evohomeclient/src/common/jsoncppbridge.hpp"

#define LOGONFAILTRESHOLD 3
#define MINPOLINTERVAL 10
#define MAXPOLINTERVAL 3600
#define HTTPTIMEOUT 30

#ifdef _WIN32
#define gmtime_r(timep, result) gmtime_s(result, timep)
#endif


#define RETURN_LOCAL_TIME true
#define RETURN_UTC_TIME false



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
	m_refreshRate(refreshrate)
{
	m_HwdID = ID;
	m_bSkipReceiveCheck = true;

	m_locationIdx = (installation >> 12) & 15;
	m_gatewayIdx = (installation >> 8) & 15;
	m_systemIdx = (installation >> 4) & 15;


	/* Use Flags
	 *
	 * 0x1 = m_updatedev (let Honeywell server control the device name)
	 * 0x2 = m_showSchedule (show next scheduled switch point as `until` time)
	 * 0x4 = m_showLocation (prefix device name with location)
	 * 0x8 = m_showhdtemps (show better precision temps from 'old' US Evohome API)
	 * 0x10 = m_precision (0 = 1 decimal, 1 = 2 decimals)
	 *
	 */

	m_updatedev = ((UseFlags & 1) == 0); // reverted value: default = true
	m_showSchedule = ((UseFlags & 2) > 0);
	m_showLocation = ((UseFlags & 4) > 0);
	m_showhdtemps = ((UseFlags & 8) > 0);
	m_hdprecision = ((UseFlags & 16) > 0) ? 2 : 1;

	if (m_refreshRate < 10)
		m_refreshRate = 60;

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
	m_szLocationName = "";

	if (m_showhdtemps)
	{
		evohome::WebAPI::v1 = new EvohomeClient();
		evohome::WebAPI::v1->set_empty_field_response("");
	}
	evohome::WebAPI::v2 = new EvohomeClient2();
	evohome::WebAPI::v2->set_empty_field_response("");
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
			int returnCode = GetLastV2ResponseCode();
			_log.Log(LOG_ERROR, "(%s) session renewal failed with message: %s (RC=%d)", m_Name.c_str(), evohome::WebAPI::v2->get_last_error().c_str(), returnCode);
			if (returnCode >= 400)
				m_loggedon = false;
			return false;
		}
	}

	if (!m_loggedon)
	{
		_log.Log(LOG_STATUS, "(%s) connect to Evohome server", m_Name.c_str());
		if (!evohome::WebAPI::v2->login(m_username, m_password))
		{
			int returnCode = GetLastV2ResponseCode();
			_log.Log(LOG_ERROR, "(%s) login failed with message: %s (RC=%d)", m_Name.c_str(), evohome::WebAPI::v2->get_last_error().c_str(), returnCode);
			m_logonfailures++;
			if (m_logonfailures == LOGONFAILTRESHOLD)
				_log.Log(LOG_STATUS, "(%s) logon fail treshold reached - trottling", m_Name.c_str());
			if ((m_logonfailures * m_refreshRate) > MAXPOLINTERVAL)
				m_logonfailures--;
			return false;
		}
		m_loggedon = true;
	}

	m_logonfailures = 0;

	// (re)initialize Evohome installation info
	std::vector<std::vector<unsigned long>>().swap(m_vUnits);
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
		m_szLocationName = evohome::WebAPI::v2->get_location_name(m_locationIdx);
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
	int refreshTimer = m_refreshRate - MINPOLINTERVAL;
	int pollcounter = LOGONFAILTRESHOLD;
	_log.Log(LOG_STATUS, "(%s) Worker started...", m_Name.c_str());
	m_lastAccessTimer=0;
	while (!IsStopRequested(1000))
	{
		refreshTimer++;
		m_lastAccessTimer++;
		if (refreshTimer % 10 == 0)
			m_LastHeartbeat = mytime(NULL);

		if ((refreshTimer % m_refreshRate == 0) && (pollcounter++ > m_logonfailures) && (m_lastAccessTimer >= MINPOLINTERVAL))
		{
			if (GetStatus())
				pollcounter = LOGONFAILTRESHOLD;
			m_lastAccessTimer=0;
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

	m_lastAccessTimer=0;
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
	if (evohome::WebAPI::v2->m_vLocations.size() < (size_t)m_locationIdx)
		return false;
	_log.Log(LOG_NORM, "(%s) fetch data from server", m_Name.c_str());
	if (!evohome::WebAPI::v2->get_status(evohome::WebAPI::v2->m_vLocations[m_locationIdx].szLocationId))
	{
		int returnCode = GetLastV2ResponseCode();
		if (returnCode >= 400)
			m_loggedon = false;
		return false;
	}

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
				evohome::device::zone* HeatingZone = &evohome::WebAPI::tcs2->zones[i];
				std::string sztemperature, szUpdateStat;
				if (m_showhdtemps)
					sztemperature = evohome::WebAPI::v1->get_zone_temperature(HeatingZone->szZoneId, m_hdprecision);

				if (sztemperature.empty())
					sztemperature = evohome::WebAPI::v2->get_zone_temperature(HeatingZone);

				if (sztemperature.empty() || (sztemperature == "128"))
					szUpdateStat = "-;-;Offline";
				else
					szUpdateStat = sztemperature + ";5;" + sznewmode;

				uint8_t unit = GetUnit_by_ID(atol(HeatingZone->szZoneId.c_str()));
				std::string szDeviceName;
				m_sql.UpdateValue(this->m_HwdID, HeatingZone->szZoneId.c_str(), unit, pTypeEvohomeZone, sTypeEvohomeZone, 10, 255, 0, szUpdateStat.c_str(), szDeviceName);
			}
			return true;
		}

		// cycle my zones to restore scheduled temps
		for (int i = 0; i < numZones; ++i)
		{
			evohome::device::zone* HeatingZone = &evohome::WebAPI::tcs2->zones[i];
			std::string zonemode = "";
			if (HeatingZone->jStatus == NULL) // don't touch invalid zone - it should already show as 'Offline'
				continue;
			if (HeatingZone->jStatus->isMember("heatSetpointStatus"))
				zonemode = (*HeatingZone->jStatus)["heatSetpointStatus"]["setpointMode"].asString();
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
			else if (sznewmode != "Custom")
			{
				if ((!HeatingZone->jSchedule.isNull()) || evohome::WebAPI::v2->get_zone_schedule(HeatingZone->szZoneId))
				{
					szuntil = evohome::WebAPI::v2->get_next_switchpoint(HeatingZone, szsetpoint, RETURN_UTC_TIME);
					setpoint = strtod(szsetpoint.c_str(), NULL);
				}

				// Eco lowers the setpoint of all zones by 3 degrees, but resets a zone mode to Normal setting
				// if the resulting setpoint is below the Away setpoint
				if ((sznewmode == "AutoWithEco") && (setpoint >= (m_awaysetpoint + 3)))
					setpoint -= 3;
			}

			if (m_showhdtemps)
				sztemperature = evohome::WebAPI::v1->get_zone_temperature(HeatingZone->szZoneId, m_hdprecision);
			else if (HeatingZone->jStatus != NULL)
				sztemperature = evohome::WebAPI::v2->get_zone_temperature(HeatingZone);

			std::string szUpdateStat;
			if ((sztemperature.empty()) || (sztemperature == "128"))
				szUpdateStat = "-;-;Offline";
			else if (sznewmode == "Custom")
				szUpdateStat = sztemperature + ";-;" + sznewmode;
			else if (setpoint < 5) // there was an error - no schedule?
				szUpdateStat = sztemperature + ";5;Unknown";
			else
			{
				std::stringstream ssSetpoint;
				ssSetpoint << setpoint;
				szUpdateStat = sztemperature + ";" + ssSetpoint.str() + ";" + sznewmode;
				if ((m_showSchedule) && !(szuntil.empty()))
					szUpdateStat.append(";" + szuntil);
			}

			uint8_t unit = GetUnit_by_ID(atol(HeatingZone->szZoneId.c_str()));
			std::string szDeviceName;
			m_sql.UpdateValue(this->m_HwdID, HeatingZone->szZoneId.c_str(), unit, pTypeEvohomeZone, sTypeEvohomeZone, 10, 255, 0, szUpdateStat.c_str(), szDeviceName);
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

	evohome::device::zone* HeatingZone = evohome::WebAPI::v2->get_zone_by_ID(zoneId);
	if (HeatingZone == NULL) // zone number not known by installation (manually added?)
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
		if ((!HeatingZone->jSchedule.isNull()) || evohome::WebAPI::v2->get_zone_schedule(HeatingZone->szZoneId))
		{
			szuntil = evohome::WebAPI::v2->get_next_switchpoint(HeatingZone, szsetpoint, RETURN_UTC_TIME);
			pEvo->temperature = (int16_t)(strtod(szsetpoint.c_str(), NULL) * 100);
		}

		if ((m_showSchedule) && (!szuntil.empty()))
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
	std::string szsystemMode;
	uint8_t sysmode = 0;

	if (tcs->jStatus == NULL)
		szsystemMode = "Unknown";
	else
		szsystemMode = evohome::WebAPI::v2->get_system_mode(tcs);
	while (sysmode < 7 && strcmp(szsystemMode.c_str(), m_szWebAPIMode[sysmode]) != 0)
		sysmode++;

	std::string szsystemModeUntil = evohome::WebAPI::v2->get_system_mode_until(tcs, RETURN_UTC_TIME);

	_tEVOHOME1 tsen;
	memset(&tsen, 0, sizeof(_tEVOHOME1));
	tsen.len = sizeof(_tEVOHOME1) - 1;
	tsen.type = pTypeEvohome;
	tsen.subtype = sTypeEvohome;
	RFX_SETID3(ID, tsen.id1, tsen.id2, tsen.id3);
	if (szsystemModeUntil.empty())
		tsen.mode = 0;
	else
	{
		// Note: standard web frontend does not allow display of the end time for temporary controller mode
		tsen.mode = 1;
		CEvohomeDateTime::DecodeISODate(tsen, szsystemModeUntil.c_str());
	}
	tsen.status = sysmode;
	sDecodeRXMessage(this, (const unsigned char *)&tsen, "Controller mode", -1);

	if (GetControllerName().empty() || m_updatedev)
	{
		std::string szmodelType = (*tcs->jInstallationInfo)["modelType"].asString();
		SetControllerName(szmodelType);
		if (szmodelType.empty())
			return;

		std::string devname;
		if (m_showLocation)
			devname = m_szLocationName + ": " + szmodelType;
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


void CEvohomeWeb::DecodeZone(evohome::device::zone* HeatingZone)
{
	// no sense in using REVOBUF EVOHOME2 to send this to mainworker as this requires breaking up our data
	// only for mainworker to reassemble it.

	std::string sztemperature, szsetpoint, szmode, szuntil;

	if (m_showhdtemps)
		sztemperature = evohome::WebAPI::v1->get_zone_temperature(HeatingZone->szZoneId, m_hdprecision);

	if (sztemperature.empty())
		sztemperature = evohome::WebAPI::v2->get_zone_temperature(HeatingZone);

	if ((sztemperature.empty()) || (sztemperature == "128"))
	{
		sztemperature = "-";
		szsetpoint = "-";
		szmode = "Offline";
	}
	else
	{
		szsetpoint = evohome::WebAPI::v2->get_zone_setpoint(HeatingZone);
		szmode = evohome::WebAPI::v2->get_zone_mode(HeatingZone);

		if (szmode == "TemporaryOverride")
			szuntil = evohome::WebAPI::v2->get_zone_mode_until(HeatingZone, RETURN_UTC_TIME);
	}

	unsigned long evoID = atol(HeatingZone->szZoneId.c_str());
	std::string szsysmode;
	if (evohome::WebAPI::tcs2->jStatus == NULL)
		szsysmode = "Unknown";
	else
		szsysmode = evohome::WebAPI::v2->get_system_mode(evohome::WebAPI::tcs2);
	if ((szsysmode == "Away") && (szmode == "FollowSchedule"))
	{
		double new_awaysetpoint = strtod(szsetpoint.c_str(), NULL);
		if (m_awaysetpoint != new_awaysetpoint)
		{
			m_awaysetpoint = new_awaysetpoint;
			m_sql.safe_query("UPDATE Hardware SET Extra='%0.2f;%d' WHERE ID=%d", m_awaysetpoint, m_wdayoff, this->m_HwdID);
			_log.Log(LOG_STATUS, "(%s) change Away setpoint to '%0.2f' because of non matching setpoint (%s)", m_Name.c_str(), m_awaysetpoint, evohome::WebAPI::v2->get_zone_name(HeatingZone).c_str());

		}
		szmode = szsysmode;
	}
	else if ((szsysmode == "HeatingOff") && (szmode != "Offline"))
		szmode = szsysmode;
	else
	{
		if (szmode == "FollowSchedule")
		{
			szmode = szsysmode;
			if (m_showSchedule && szuntil.empty())
			{
				if (szsysmode != "Custom") // can't use schedule to find next switchpoint
					szuntil = evohome::WebAPI::v2->get_next_switchpoint(HeatingZone, RETURN_UTC_TIME);
			}
			if (szsysmode == "DayOff")
			{
				if (szuntil.empty()) // make sure our schedule is populated
					evohome::WebAPI::v2->get_next_switchpoint(HeatingZone);
				if (szsetpoint != HeatingZone->jSchedule["currentSetpoint"].asString())
				{
					bool found = false;
					m_wdayoff--;
					for (uint8_t i = 0; (i < 7) && !found; i++)
					{
						HeatingZone->jSchedule["nextSwitchpoint"] = ""; // force a recalculation
						m_wdayoff++;
						if (m_wdayoff>6)
							m_wdayoff -= 7;
						evohome::WebAPI::v2->get_next_switchpoint(HeatingZone);
						found = (szsetpoint == HeatingZone->jSchedule["currentSetpoint"].asString());
					}
					if (found)
					{
						m_sql.safe_query("UPDATE Hardware SET Extra='%0.2f;%d' WHERE ID=%d", m_awaysetpoint, m_wdayoff, this->m_HwdID);

						_log.Log(LOG_STATUS, "(%s) change Day Off schedule reference to '%s' because of non matching setpoint (%s)", m_Name.c_str(), weekdays[m_wdayoff].c_str(), evohome::WebAPI::v2->get_zone_name(HeatingZone).c_str());
					}
					if (m_showSchedule)
						szuntil = evohome::WebAPI::v2->get_next_switchpoint(HeatingZone, RETURN_UTC_TIME);
				}
			}
		}
	}

	std::string szUpdateStat = sztemperature + ";" + szsetpoint + ";" + szmode;
	if (!szuntil.empty())
		szUpdateStat.append(";" + szuntil);

	std::string szDeviceName;
	uint64_t DevRowIdx = m_sql.UpdateValue(this->m_HwdID, HeatingZone->szZoneId.c_str(), GetUnit_by_ID(evoID), pTypeEvohomeZone, sTypeEvohomeZone, 10, 255, 0, szUpdateStat.c_str(), szDeviceName);

	if (m_updatedev && (DevRowIdx != -1))
	{
		std::string szZoneName;
		if (m_showLocation)
			szZoneName = m_szLocationName + ": ";
		szZoneName.append(evohome::WebAPI::v2->get_zone_name(HeatingZone));

		if (szDeviceName != szZoneName)
		{
			// also wipe StrParam1 - we do not want a double action from the old (python) script when changing the setpoint
			m_sql.safe_query("UPDATE DeviceStatus SET Name='%q', StrParam1='' WHERE (ID == %" PRIu64 ")", szZoneName.c_str(), DevRowIdx);
			if (szDeviceName.find("zone ") != std::string::npos)
				_log.Log(LOG_STATUS, "(%s) register new zone '%s'", m_Name.c_str(), szZoneName.c_str());
			szDeviceName = szZoneName;
		}
	}

	// Notify MQTT and various push mechanisms
	m_mainworker.sOnDeviceReceived(this->m_HwdID, DevRowIdx, szDeviceName, NULL);
}


void CEvohomeWeb::DecodeDHWState(evohome::device::temperatureControlSystem* tcs)
{
	if (!evohome::WebAPI::v2->has_dhw(tcs))
		return;

	// Hot Water is essentially just another zone
	evohome::device::zone* HotWater = &(tcs->dhw[0]);

	if (!(*HotWater->jStatus).isMember("temperatureStatus") || !(*HotWater->jStatus).isMember("stateStatus"))
		return;

	std::string sztemperature, szstate, szmode, szuntil;
	std::stringstream ssUpdateStat;

	if (m_showhdtemps)
		sztemperature = evohome::WebAPI::v1->get_zone_temperature(HotWater->szZoneId, m_hdprecision);

	if (sztemperature.empty())
		sztemperature = evohome::WebAPI::v2->get_zone_temperature(HotWater);

	szstate = (*HotWater->jStatus)["stateStatus"]["state"].asString();

	if (sztemperature.empty() || (sztemperature == "128") || szstate.empty())
	{
		sztemperature = "-";
		szstate = "-";
		szmode = "Offline";
	}
	else
	{
		szmode = (*HotWater->jStatus)["stateStatus"]["mode"].asString();
		if (szmode == "FollowSchedule")
			szmode = "Auto";

		if (szmode == "TemporaryOverride")
			szuntil = (*HotWater->jStatus)["stateStatus"]["until"].asString();
		else if (m_showSchedule)
		{
			std::string szsysmode = evohome::WebAPI::v2->get_system_mode(tcs);
			if (szsysmode != "Custom") // can't use schedule to find next switchpoint
			{
				szuntil = evohome::WebAPI::v2->get_next_switchpoint(HotWater, RETURN_UTC_TIME);
			}
		}
	}

	std::string szUpdateStat = sztemperature + ";" + szstate+ ";" + szmode;
	if (!szuntil.empty())
		szUpdateStat.append(";" + szuntil);

	std::string szDeviceName;
	uint64_t DevRowIdx = m_sql.UpdateValue(this->m_HwdID, HotWater->szZoneId.c_str(), 1, pTypeEvohomeWater, sTypeEvohomeWater, 10, 255, 50, szUpdateStat.c_str(), szDeviceName);

	if (m_updatedev && (DevRowIdx != -1))
	{
		std::string szZoneName;
		if (m_showLocation)
			szZoneName = m_szLocationName + ": ";
		szZoneName.append("Hot Water");

		if (szDeviceName != szZoneName)
		{
			// also wipe StrParam1 - we do not want a double action from the old (python) script when changing the setpoint
			m_sql.safe_query("UPDATE DeviceStatus SET Name='%q', StrParam1='' WHERE (ID == %" PRIu64 ")", szZoneName.c_str(), DevRowIdx);
			szDeviceName = szZoneName;
		}
	}

	// Notify MQTT and various push mechanisms
	m_mainworker.sOnDeviceReceived(this->m_HwdID, DevRowIdx, szDeviceName, NULL);
}


/*
 * Code for serial and python scripts appear to assume that zones are always returned in the same order
 * I'm not sure that is really true, so I'll use a vector to match the evohome ID and our unit number.
 */
uint8_t CEvohomeWeb::GetUnit_by_ID(unsigned long evoID)
{
	if (m_vUnits.size() <= (size_t)m_locationIdx)
	{
		while (m_vUnits.size() <= (size_t)m_locationIdx)
		{
			std::vector<unsigned long> newLocation;
			newLocation.push_back(1);
			for (uint8_t unit = 0; unit < m_nMaxZones; unit++)
				newLocation.push_back(0);
			m_vUnits.push_back(newLocation);
		}
	}
	else
	{
		for (uint8_t unit = 1; unit <= m_nMaxZones; unit++)
		{
			if (m_vUnits[m_locationIdx][unit] == evoID)
				return unit;
		}
	}

	uint8_t found = 0;
	if (evohome::WebAPI::v2->is_single_heating_system())
	{
		std::vector<std::vector<std::string>> result;
		result = m_sql.safe_query(
			"SELECT Unit,DeviceID FROM DeviceStatus WHERE (HardwareID = %d) AND (Type = %d) ORDER BY Unit",
			this->m_HwdID, pTypeEvohomeZone);

		uint8_t numresults = static_cast<uint8_t>(result.size());
		for (uint8_t row = 0; row < numresults; row++)
		{
			int unit = atoi(result[row][0].c_str());
			m_vUnits[m_locationIdx][unit] = static_cast<unsigned long>(atol(result[row][1].c_str()));
			if (m_vUnits[m_locationIdx][unit] == evoID)
				found = unit;
			else if (m_vUnits[m_locationIdx][unit] == (unsigned long)(unit + 92000)) // mark manually added, unlinked zone as free
				m_vUnits[m_locationIdx][unit] = 0;
		}
	}
	else
	{
		std::vector<std::vector<std::string>> result;
		result = m_sql.safe_query(
			"SELECT Unit FROM DeviceStatus WHERE (HardwareID = %d) AND (Type = %d) AND (DeviceID = '%lu')",
			this->m_HwdID, pTypeEvohomeZone, evoID);
		if (result.size() > 0)
		{
			int unit = atoi(result[0][0].c_str());
			if (unit == 0)
			{
				while ((m_vUnits[m_locationIdx][unit] != 0) && (unit <= m_nMaxZones))
					unit++;
				if (unit > m_nMaxZones)
				{
					_log.Log(LOG_ERROR, "(%s) cannot create a valid zone unit number because you have no free zones left in this location", m_Name.c_str());
					return 0;
				}

				m_sql.safe_query("UPDATE DeviceStatus SET Unit='%d' WHERE (HardwareID = %d) AND (Type = %d) AND (DeviceID = '%lu')", unit, this->m_HwdID, pTypeEvohomeZone, evoID);
			}
			m_vUnits[m_locationIdx][unit] = evoID;
			found = unit;
		}
	}

	if ((found == 0) && m_updatedev) // create zone device
	{
		std::string szDeviceName;
		char cDeviceID[40];
		sprintf(cDeviceID, "%lu", evoID);
		uint8_t unit = 0;

		if (evohome::WebAPI::v2->is_single_heating_system()) // create zone in first free unit
		{
			unit = 1;		
			while ((m_vUnits[m_locationIdx][unit] != 0) && (unit <= m_nMaxZones))
				unit++;

			if (unit > m_nMaxZones)
			{
				_log.Log(LOG_ERROR, "(%s) cannot add new zone because you have no free zones left in this location", m_Name.c_str());
				return 0;
			}
			found = unit;
		}
		// else create as unit zero -> we'll update that on the next visit

		uint64_t DevRowIdx = m_sql.UpdateValue(this->m_HwdID, cDeviceID, unit, pTypeEvohomeZone, sTypeEvohomeZone, 10, 255, 0, "0.0;0.0;Auto", szDeviceName);
		if (DevRowIdx != -1)
		{
			char devname[8];
			sprintf(devname, "zone %d", (int)found);
			m_sql.safe_query("UPDATE DeviceStatus SET Name='%q' WHERE (ID == %" PRIu64 ")", devname, DevRowIdx);
		}
	}

	return found;
}




