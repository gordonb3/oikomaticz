/*
 * Copyright (c) 2016-2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for UK/EMEA Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <cstdlib>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>

#include "API2.hpp"
#include "evohomeclient2.hpp"
#include "../connection/EvoHTTPBridge.hpp"
#include "../common/jsoncppbridge.hpp"
#include "../time/IsoTimeString.hpp"


#ifdef _WIN32
#define localtime_r(timep, result) localtime_s(result, timep)
#define gmtime_r(timep, result) gmtime_s(result, timep)
#endif

#ifndef _WIN32
#define sprintf_s(buffer, buffer_size, stringbuffer, ...) (sprintf(buffer, stringbuffer, __VA_ARGS__))
#endif


/*
 * Class construct
 */
EvohomeClient2::EvohomeClient2()
{
	init();
}


EvohomeClient2::~EvohomeClient2()
{
	cleanup();
}


/*
 * Initialize
 */
/* private */ void EvohomeClient2::init()
{
	m_szEmptyFieldResponse = "<null>";
}


/*
 * Cleanup curl web client
 */
void EvohomeClient2::cleanup()
{
	EvoHTTPBridge::CloseConnection();
}


std::string EvohomeClient2::get_last_error()
{
	return m_szLastError;
}


std::string EvohomeClient2::get_last_response()
{
	return m_szResponse;
}


void EvohomeClient2::set_empty_field_response(std::string szResponse)
{
	m_szEmptyFieldResponse = szResponse;
}


/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 ************************************************************************/


/* private */ bool EvohomeClient2::obtain_access_token(const std::string &szCredentials)
{
	std::vector<std::string> vLoginHeader;
	vLoginHeader.push_back(evohome::API2::header::authkey);
	vLoginHeader.push_back(evohome::API2::header::accept);

	std::string szPostdata = "Content-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8&Host=rs.alarmnet.com%2F";
	szPostdata.append("&Cache-Control=no-store%20no-cache&Pragma=no-cache&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous&Connection=Keep-Alive&");
	szPostdata.append(szCredentials);

	std::string szUrl = EVOHOME_HOST"/Auth/OAuth/Token";
	EvoHTTPBridge::SafePOST(szUrl, szPostdata, vLoginHeader, m_szResponse, -1);

	Json::Value jLogin;
	if (evohome::parse_json_string(m_szResponse, jLogin) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	std::string szError = "";
	if (jLogin.isMember("error"))
		szError = jLogin["error"].asString();
	if (jLogin.isMember("message"))
		szError = jLogin["message"].asString();
	if (!szError.empty())
	{
		m_szLastError = "login returned ";
		m_szLastError.append(szError);
		return false;
	}

	m_szAccessToken = jLogin["access_token"].asString();
	m_szRefreshToken = jLogin["refresh_token"].asString();
	m_tTokenExpirationTime = time(NULL) + atoi(jLogin["expires_in"].asString().c_str());
	std::string szAuthBearer = "Authorization: bearer ";
	szAuthBearer.append(m_szAccessToken);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back(evohome::API2::header::accept);
	m_vEvoHeader.push_back(evohome::API2::header::jsondata);

	return get_user_id();
}


/*
 * Login to the evohome portal
 */
bool EvohomeClient2::login(const std::string &szUser, const std::string &szPassword)
{
	std::string szCredentials = "grant_type=password&Username=";
	szCredentials.append(EvoHTTPBridge::URLEncode(szUser));
	szCredentials.append("&Password=");
	szCredentials.append(EvoHTTPBridge::URLEncode(szPassword));

	return obtain_access_token(szCredentials);
}


/*
 * Renew the Authorization token
 */
bool EvohomeClient2::renew_login()
{
	if (m_szRefreshToken.empty())
		return false;

	std::string szCredentials = "grant_type=refresh_token&refresh_token=";
	szCredentials.append(m_szRefreshToken);

	return obtain_access_token(szCredentials);
}


/*
 * Save authorization key to a backup file
 */
bool EvohomeClient2::save_auth_to_file(const std::string &szFilename)
{
	std::ofstream myfile (szFilename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value jAuth;

		jAuth["access_token"] = m_szAccessToken;
		jAuth["refresh_token"] = m_szRefreshToken;
		jAuth["expiration_time"] = static_cast<unsigned int>(m_tTokenExpirationTime);

		myfile << jAuth.toStyledString() << "\n";
		myfile.close();
		return true;
	}
	return false;
}


bool EvohomeClient2::is_session_valid()
{
	return (m_tTokenExpirationTime > time(NULL));
}


/*
 * Load authorization key from a backup file
 */
bool EvohomeClient2::load_auth_from_file(const std::string &szFilename)
{
	std::string szFileContent;
	std::ifstream myfile (szFilename.c_str());
	if ( myfile.is_open() )
	{
		std::string line;
		while ( getline (myfile,line) )
		{
			szFileContent.append(line);
			szFileContent.append("\n");
		}
		myfile.close();
	}
	Json::Value jAuth;
	if (evohome::parse_json_string(szFileContent, jAuth) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	m_szAccessToken = jAuth["access_token"].asString();
	m_szRefreshToken = jAuth["refresh_token"].asString();
	m_tTokenExpirationTime = static_cast<time_t>(atoi(jAuth["expiration_time"].asString().c_str()));

	if (!is_session_valid())
	{
		bool bRenew = renew_login();
		if (bRenew)
			save_auth_to_file(szFilename);
		return bRenew;
	}

	std::string szAuthBearer = "Authorization: bearer ";
	szAuthBearer.append(m_szAccessToken);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back(evohome::API2::header::accept);
	m_vEvoHeader.push_back(evohome::API2::header::jsondata);

	return get_user_id();
}


/*
 * Retrieve evohome user info
 */
bool EvohomeClient2::get_user_id()
{
	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::userAccount);
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, m_szResponse, -1);

	Json::Value jUserAccount;
	if (evohome::parse_json_string(m_szResponse, jUserAccount) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	m_szUserId = jUserAccount["userId"].asString();
	return true;
}


/************************************************************************
 *									*
 *	Evohome heating installations retrieval				*
 *									*
 ************************************************************************/


/* private */ void EvohomeClient2::get_dhw(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx)
{
	evohome::device::temperatureControlSystem *myTCS = &m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[systemIdx];

	std::vector<evohome::device::zone>().swap((*myTCS).dhw);

	if (!(*myTCS->jInstallationInfo).isMember("dhw"))
		return;

	Json::Value *jTCS = (*myTCS).jInstallationInfo;
	(*myTCS).dhw.resize(1);
	(*myTCS).dhw[0].jInstallationInfo = &(*jTCS)["dhw"];
	(*myTCS).dhw[0].szZoneId = (*jTCS)["dhw"]["dhwId"].asString();
	(*myTCS).dhw[0].zoneIdx = 128;
	(*myTCS).dhw[0].systemIdx = systemIdx;
	(*myTCS).dhw[0].gatewayIdx = gatewayIdx;
	(*myTCS).dhw[0].locationIdx = locationIdx;

	evohome::device::path::zone newzonepath = evohome::device::path::zone();
	newzonepath.locationIdx = locationIdx;
	newzonepath.gatewayIdx = gatewayIdx;
	newzonepath.systemIdx = systemIdx;
	newzonepath.zoneIdx = 128;
	newzonepath.szZoneId = (*myTCS).dhw[0].szZoneId;
	m_vZonePaths.push_back(newzonepath);
}


/* private */ void EvohomeClient2::get_zones(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx)
{
	evohome::device::temperatureControlSystem *myTCS = &m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[systemIdx];

	std::vector<evohome::device::zone>().swap((*myTCS).zones);
	Json::Value *jTCS = (*myTCS).jInstallationInfo;

	if (!(*jTCS)["zones"].isArray())
		return;

	int l = static_cast<int>((*jTCS)["zones"].size());
	(*myTCS).zones.resize(l);
	for (int i = 0; i < l; ++i)
	{
		(*myTCS).zones[i].jInstallationInfo = &(*jTCS)["zones"][i];
		(*myTCS).zones[i].szZoneId = (*jTCS)["zones"][i]["zoneId"].asString();
		(*myTCS).zones[i].zoneIdx = i;
		(*myTCS).zones[i].systemIdx = systemIdx;
		(*myTCS).zones[i].gatewayIdx = gatewayIdx;
		(*myTCS).zones[i].locationIdx = locationIdx;

		evohome::device::path::zone newzonepath = evohome::device::path::zone();
		newzonepath.locationIdx = locationIdx;
		newzonepath.gatewayIdx = gatewayIdx;
		newzonepath.systemIdx = systemIdx;
		newzonepath.zoneIdx = i;
		newzonepath.szZoneId = (*myTCS).zones[i].szZoneId;
		m_vZonePaths.push_back(newzonepath);
	}
}


/* private */ void EvohomeClient2::get_temperatureControlSystems(const unsigned int locationIdx, const unsigned int gatewayIdx)
{

	evohome::device::gateway *myGateway = &m_vLocations[locationIdx].gateways[gatewayIdx];

	std::vector<evohome::device::temperatureControlSystem>().swap((*myGateway).temperatureControlSystems);
	Json::Value *jGateway = (*myGateway).jInstallationInfo;

	if (!(*jGateway)["temperatureControlSystems"].isArray())
		return;

	int l = static_cast<int>((*jGateway)["temperatureControlSystems"].size());
	(*myGateway).temperatureControlSystems.resize(l);
	for (int i = 0; i < l; ++i)
	{
		(*myGateway).temperatureControlSystems[i].jInstallationInfo = &(*jGateway)["temperatureControlSystems"][i];
		(*myGateway).temperatureControlSystems[i].szSystemId = (*jGateway)["temperatureControlSystems"][i]["systemId"].asString();
		(*myGateway).temperatureControlSystems[i].locationIdx = locationIdx;
		(*myGateway).temperatureControlSystems[i].gatewayIdx = gatewayIdx;
		(*myGateway).temperatureControlSystems[i].systemIdx = i;

		get_zones(locationIdx, gatewayIdx, i);
		get_dhw(locationIdx, gatewayIdx, i);
	}
}


/* private */ void EvohomeClient2::get_gateways(const unsigned int locationIdx)
{
	std::vector<evohome::device::gateway>().swap(m_vLocations[locationIdx].gateways);
	Json::Value *jLocation = m_vLocations[locationIdx].jInstallationInfo;

	if (!(*jLocation)["gateways"].isArray())
		return;

	int l = static_cast<int>((*jLocation)["gateways"].size());
	m_vLocations[locationIdx].gateways.resize(l);
	for (int i = 0; i < l; ++i)
	{
		m_vLocations[locationIdx].gateways[i].jInstallationInfo = &(*jLocation)["gateways"][i];
		m_vLocations[locationIdx].gateways[i].szGatewayId = (*jLocation)["gateways"][i]["gatewayInfo"]["gatewayId"].asString();
		m_vLocations[locationIdx].gateways[i].locationIdx = locationIdx;
		m_vLocations[locationIdx].gateways[i].gatewayIdx = i;

		get_temperatureControlSystems(locationIdx, i);
	}
}


/*
 * Retrieve evohome installation info
 */
bool EvohomeClient2::full_installation()
{
	std::vector<evohome::device::location>().swap(m_vLocations);
	std::vector<evohome::device::path::zone>().swap(m_vZonePaths);

	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::installationInfo, m_szUserId);
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, m_szResponse, -1);

	// evohome API returns an unnamed json array which is not accepted by our parser
	m_szResponse.insert(0, "{\"locations\": ");
	m_szResponse.append("}");

	m_jFullInstallation.clear();
	if (evohome::parse_json_string(m_szResponse, m_jFullInstallation) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	int l = static_cast<int>(m_jFullInstallation["locations"].size());
	for (int i = 0; i < l; ++i)
	{
		evohome::device::location newloc = evohome::device::location();
		m_vLocations.push_back(newloc);
		m_vLocations[i].jInstallationInfo = &m_jFullInstallation["locations"][i];
		m_vLocations[i].szLocationId = m_jFullInstallation["locations"][i]["locationInfo"]["locationId"].asString();
		m_vLocations[i].locationIdx = i;

		get_gateways(i);
	}
	return true;
}


/************************************************************************
 *									*
 *	Evohome system status retrieval					*
 *									*
 ************************************************************************/


/*
 * Retrieve evohome status info
 */
bool EvohomeClient2::get_status(const unsigned int locationIdx)
{
	if (locationIdx >= static_cast<unsigned int>(m_vLocations.size()))
	{
		m_szLastError = "Invalid location ID";
		return false;
	}

	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::status, m_vLocations[locationIdx].szLocationId);
	if (!EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, m_szResponse, -1))
	{
		m_szLastError = "HTTP error during fetch status";
		return false;
	}

	m_vLocations[locationIdx].jStatus.clear();
	if (evohome::parse_json_string(m_szResponse, m_vLocations[locationIdx].jStatus) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}
	Json::Value *jLocation = &m_vLocations[locationIdx].jStatus;

	// get gateway status
	if (!(*jLocation)["gateways"].isArray())
	{
		m_szLastError = "No gateway found";
		return false;
	}

	int lgw = static_cast<int>((*jLocation)["gateways"].size());
	for (int igw = 0; igw < lgw; ++igw)
	{
		std::string szGatewayId = (*jLocation)["gateways"][igw]["gatewayId"].asString();
		evohome::device::gateway *_tGateway = get_gateway_by_ID(szGatewayId);
		_tGateway->jStatus = &(*jLocation)["gateways"][igw];

		// get temperatureControlSystem status
		if (!(*_tGateway->jStatus)["temperatureControlSystems"].isArray())
		{
			m_szLastError = "No temperature control system found";
			return false;
		}

		int ltcs = static_cast<int>((*_tGateway->jStatus)["temperatureControlSystems"].size());
		for (int itcs = 0; itcs < ltcs; itcs++)
		{
			std::string szSystemId = (*_tGateway->jStatus)["temperatureControlSystems"][itcs]["systemId"].asString();
			evohome::device::temperatureControlSystem *_tTCS = get_temperatureControlSystem_by_ID(szSystemId);
			_tTCS->jStatus = &(*_tGateway->jStatus)["temperatureControlSystems"][itcs];

			// get zone status
			if (!(*_tTCS->jStatus)["zones"].isArray())
			{
				m_szLastError = "No zones found";
				return false;
			}

			int lz = static_cast<int>((*_tTCS->jStatus)["zones"].size());
			for (int iz = 0; iz < lz; iz++)
			{
				std::string szZoneId = (*_tTCS->jStatus)["zones"][iz]["zoneId"].asString();
				evohome::device::zone *_tZone = get_zone_by_ID(szZoneId);
				_tZone->jStatus = &(*_tTCS->jStatus)["zones"][iz];
			}

			if (has_dhw(_tTCS))
			{
				_tTCS->dhw[0].jStatus = &(*_tTCS->jStatus)["dhw"];
			}
		}
	}
	return true;
}


bool EvohomeClient2::get_status(std::string szLocationId)
{
	if (m_vLocations.size() == 0)
		return false;
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int i = 0; i < numLocations; i++)
	{
		if (m_vLocations[i].szLocationId == szLocationId)
			return get_status(i);
	}
	return false;
}


/************************************************************************
 *									*
 *	Locate Evohome elements						*
 *									*
 ************************************************************************/


int EvohomeClient2::get_location_index(const std::string szLocationId)
{
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int iloc = 0; iloc < numLocations; iloc++)
	{
		if (m_vLocations[iloc].szLocationId == szLocationId)
			return iloc;
	}
	return -1;
}


/* private */ evohome::device::path::zone *EvohomeClient2::get_zone_path(const std::string szZoneId)
{
	int iz = get_zone_path_ID(szZoneId);
	if (iz < 0)
		return NULL;
	return &m_vZonePaths[iz];
}


/* private */ int EvohomeClient2::get_zone_path_ID(const std::string szZoneId)
{
	int numZones = static_cast<int>(m_vZonePaths.size());
	for (int iz = 0; iz < numZones; iz++)
	{
		if (m_vZonePaths[iz].szZoneId == szZoneId)
			return iz;
	}
	return -1;
}


evohome::device::zone *EvohomeClient2::get_zone_by_ID(std::string szZoneId)
{
	evohome::device::path::zone *zp = get_zone_path(szZoneId);
	if (zp == NULL)
		return NULL;

	if (zp->zoneIdx & 128)
		return &m_vLocations[zp->locationIdx].gateways[zp->gatewayIdx].temperatureControlSystems[zp->systemIdx].dhw[0];
	else
		return &m_vLocations[zp->locationIdx].gateways[zp->gatewayIdx].temperatureControlSystems[zp->systemIdx].zones[zp->zoneIdx];
}


evohome::device::zone *EvohomeClient2::get_zone_by_Name(std::string szZoneName)
{
	int numZones = static_cast<int>(m_vZonePaths.size());
	for (int iz = 0; iz < numZones; iz++)
	{
		evohome::device::path::zone *zp = &m_vZonePaths[iz];
		evohome::device::zone *myZone = &m_vLocations[zp->locationIdx].gateways[zp->gatewayIdx].temperatureControlSystems[zp->systemIdx].zones[zp->zoneIdx];
		if (!(zp->zoneIdx & 128) && ((*myZone->jInstallationInfo)["name"].asString() == szZoneName))
			return myZone;
	}
	return NULL;
}


evohome::device::location *EvohomeClient2::get_location_by_ID(std::string szLocationId)
{
	if (m_vLocations.size() == 0)
		full_installation();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		if (m_vLocations[il].szLocationId == szLocationId)
			return &m_vLocations[il];
	}
	return NULL;
}


evohome::device::gateway *EvohomeClient2::get_gateway_by_ID(std::string szGatewayId)
{
	if (m_vLocations.size() == 0)
		full_installation();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
		for (int igw = 0; igw < numGateways; igw++)
		{
			if (m_vLocations[il].gateways[igw].szGatewayId == szGatewayId)
				return &m_vLocations[il].gateways[igw];
		}
	}
	return NULL;
}


evohome::device::temperatureControlSystem *EvohomeClient2::get_temperatureControlSystem_by_ID(std::string szSystemId)
{
	if (m_vLocations.size() == 0)
		full_installation();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
		for (int igw = 0; igw < numGateways; igw++)
		{
			int numTCSs = static_cast<int>(m_vLocations[il].gateways[igw].temperatureControlSystems.size());
			for (int itcs = 0; itcs < numTCSs; itcs++)
			{
				if (m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].szSystemId == szSystemId)
					return &m_vLocations[il].gateways[igw].temperatureControlSystems[itcs];
			}
		}
	}
	return NULL;
}


evohome::device::temperatureControlSystem *EvohomeClient2::get_zone_temperatureControlSystem(evohome::device::zone *zone)
{
	evohome::device::path::zone *zp = get_zone_path(zone->szZoneId);
	if (zp == NULL)
		return NULL;

	return &m_vLocations[zp->locationIdx].gateways[zp->gatewayIdx].temperatureControlSystems[zp->systemIdx];
}


/************************************************************************
 *									*
 *	Schedule handlers						*
 *									*
 ************************************************************************/


/*
 * Retrieve a zone's next switchpoint
 *
 * Returns ISO datatime string relative to UTC (timezone 'Z')
 */
std::string EvohomeClient2::request_next_switchpoint(const std::string szZoneId)
{
	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::zoneUpcoming, szZoneId, 0);
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, m_szResponse, -1);

	Json::Value jSwitchPoint;
	if (evohome::parse_json_string(m_szResponse, jSwitchPoint) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return m_szEmptyFieldResponse;
	}

	std::string szSwitchpoint = jSwitchPoint["time"].asString();
	szSwitchpoint.append("Z");
	return szSwitchpoint;
}


/*
 * Retrieve a zone's schedule
 */
bool EvohomeClient2::get_zone_schedule(const std::string szZoneId)
{
	return get_zone_schedule_ex(szZoneId, 0);
}
bool EvohomeClient2::get_dhw_schedule(const std::string szDHWId)
{
	return get_zone_schedule_ex(szDHWId, 1);
}
/* private */ bool EvohomeClient2::get_zone_schedule_ex(const std::string szZoneId, const unsigned int zoneType)
{

	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::zoneSchedule, szZoneId, zoneType);
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, m_szResponse, -1);

	if (!m_szResponse.find("\"id\""))
		return false;
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return false;

	if (evohome::parse_json_string(m_szResponse, myZone->jSchedule) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}
	return true;
}


/*
 * Find a zone's next switchpoint (localtime)
 *
 * Returns ISO datatime string relative to localtime (hardcoded as timezone 'A')
 * Extended function also fills szCurrentSetpoint with the current target temperature
 */
std::string EvohomeClient2::get_next_switchpoint(const std::string szZoneId)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;
	return get_next_switchpoint(myZone);
}
std::string EvohomeClient2::get_next_switchpoint(evohome::device::zone *zone, bool bLocaltime)
{
	std::string szCurrentSetpoint;
	return get_next_switchpoint(zone, szCurrentSetpoint, -1, bLocaltime);
}
std::string EvohomeClient2::get_next_switchpoint(evohome::device::zone *zone, std::string &szCurrentSetpoint, const bool bLocaltime)
{
	return get_next_switchpoint(zone, szCurrentSetpoint, -1, bLocaltime);
}
std::string EvohomeClient2::get_next_switchpoint(evohome::device::zone *zone, std::string &szCurrentSetpoint, const int force_weekday, const bool bLocaltime)
{
	if (zone->jSchedule.isNull())
	{
		int zoneType = ((*zone->jInstallationInfo).isMember("dhwId")) ? 1 : 0;
		if (!get_zone_schedule_ex(zone->szZoneId, zoneType))
			return m_szEmptyFieldResponse;
	}

	Json::Value *jSchedule = &(zone->jSchedule);
	int numSchedules = static_cast<int>((*jSchedule)["dailySchedules"].size());
	if (numSchedules == 0)
		return m_szEmptyFieldResponse;

	struct tm ltime;
	time_t now = time(0);
	localtime_r(&now, &ltime);
	int currentYear = ltime.tm_year;
	int currentMonth = ltime.tm_mon;
	int currentDay = ltime.tm_mday;
	int currentWeekday = (force_weekday >= 0) ? (force_weekday % 7) : ltime.tm_wday;
	char cDate[30];
	sprintf_s(cDate, 30, "%04d-%02d-%02dT%02d:%02d:%02dA", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	std::string szCurrentTime = std::string(cDate);

	std::string szNextTime = (*jSchedule)["nextSwitchpoint"].asString();
	if (szCurrentTime <= szNextTime) // our current cached values are still valid
	{
		szCurrentSetpoint = (*jSchedule)["currentSetpoint"].asString();
		if (!bLocaltime)
			return IsoTimeString::local_to_utc(szNextTime);
		return szNextTime;
	}

	std::string szTime;
	bool found = false;
	szCurrentSetpoint = "";
	for (uint8_t addDays = 0; ((addDays < 7) && !found); addDays++)
	{
		int tryDay = (currentWeekday + addDays) % 7;
		Json::Value *jDaySchedule;
		// find day
		for (int i = 0; ((i < numSchedules) && !found); i++)
		{
			jDaySchedule = &(*jSchedule)["dailySchedules"][i];
			if (((*jDaySchedule).isMember("dayOfWeek")) && ((*jDaySchedule)["dayOfWeek"] == evohome::schedule::dayOfWeek[tryDay]))
				found = true;
		}
		if (!found)
			continue;

		found = false;
		int numSwitchpoints = static_cast<int>((*jDaySchedule)["switchpoints"].size());
		for (int i = 0; ((i < numSwitchpoints) && !found); ++i)
		{
			Json::Value *jSwitchpoint = &(*jDaySchedule)["switchpoints"][i];
			szTime = (*jSwitchpoint)["timeOfDay"].asString();
			ltime.tm_isdst = -1;
			ltime.tm_year = currentYear;
			ltime.tm_mon = currentMonth;
			ltime.tm_mday = currentDay + addDays;
			ltime.tm_hour = std::atoi(szTime.substr(0, 2).c_str());
			ltime.tm_min = std::atoi(szTime.substr(3, 2).c_str());
			ltime.tm_sec = std::atoi(szTime.substr(6, 2).c_str());
			time_t ntime = mktime(&ltime);
			if (ntime > now)
				found = true;
			else if ((*jSwitchpoint).isMember("temperature"))
				szCurrentSetpoint = (*jSwitchpoint)["temperature"].asString();
			else
				szCurrentSetpoint = (*jSwitchpoint)["dhwState"].asString();
		}
	}

	if (szCurrentSetpoint.empty()) // got a direct match for the next switchpoint, need to go back in time to find the current setpoint
	{
		found = false;
		for (uint8_t subtractDays = 1; ((subtractDays < 7) && !found); subtractDays++)
		{
			int tryDay = (currentWeekday - subtractDays + 7) % 7;
			Json::Value *jDaySchedule;
			// find day
			for (int i = 0; ((i < numSchedules) && !found); i++)
			{
				jDaySchedule = &(*jSchedule)["dailySchedules"][i];
				if (((*jDaySchedule).isMember("dayOfWeek")) && ((*jDaySchedule)["dayOfWeek"] == evohome::schedule::dayOfWeek[tryDay]))
					found = true;
			}
			if (!found)
				continue;

			found = false;
			int j = static_cast<int>((*jDaySchedule)["switchpoints"].size());
			if (j > 0)
			{
				j--;
				if ((*jDaySchedule)["switchpoints"][j].isMember("temperature"))
					szCurrentSetpoint = (*jDaySchedule)["switchpoints"][j]["temperature"].asString();
				else
					szCurrentSetpoint = (*jDaySchedule)["switchpoints"][j]["dhwState"].asString();
				found = true;
			}
		}
	}

	if (!found)
		return m_szEmptyFieldResponse;

	sprintf_s(cDate, 30, "%04d-%02d-%02dT%sA", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, szTime.c_str()); // localtime => use CET to indicate that it is not UTC
	szNextTime = std::string(cDate);
	(*jSchedule)["currentSetpoint"] = szCurrentSetpoint;
	(*jSchedule)["nextSwitchpoint"] = szNextTime;
	if (!bLocaltime)
		return IsoTimeString::local_to_utc(szNextTime);
	return szNextTime;
}


/*
 * Backup all schedules to a file
 */
bool EvohomeClient2::schedules_backup(const std::string &szFilename)
{
	std::ofstream myfile (szFilename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value jBackupSchedule;

		int numLocations = static_cast<int>(m_vLocations.size());
		for (int il = 0; il < numLocations; il++)
		{
			Json::Value *jLocation = m_vLocations[il].jInstallationInfo;
			std::string szLocationId = (*jLocation)["locationInfo"]["locationId"].asString();
			if (szLocationId.empty())
				continue;

			Json::Value jBackupScheduleLocation;
			jBackupScheduleLocation["locationId"] = szLocationId;
			jBackupScheduleLocation["name"] = (*jLocation)["locationInfo"]["name"].asString();

			int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
			for (int igw = 0; igw < numGateways; igw++)
			{
				Json::Value *jGateway = m_vLocations[il].gateways[igw].jInstallationInfo;
				std::string szGatewayId = (*jGateway)["gatewayInfo"]["gatewayId"].asString();
				if (szGatewayId.empty())
					continue;

				Json::Value jBackupScheduleGateway;
				jBackupScheduleGateway["gatewayId"] = szGatewayId;

				int numTCSs = static_cast<int>(m_vLocations[il].gateways[igw].temperatureControlSystems.size());
				for (int itcs = 0; itcs < numTCSs; itcs++)
				{
					Json::Value *jTCS = m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].jInstallationInfo;
					std::string szTCSId = (*jTCS)["systemId"].asString();

					if (szTCSId.empty())
						continue;

					Json::Value jBackupScheduleTCS;
					jBackupScheduleTCS["systemId"] = szTCSId;
					if (!(*jTCS)["zones"].isArray())
						continue;

					int numZones = static_cast<int>((*jTCS)["zones"].size());
					for (int iz = 0; iz < numZones; iz++)
					{
						std::string szZoneId = (*jTCS)["zones"][iz]["zoneId"].asString();
						if (szZoneId.empty())
							continue;

						std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::zoneSchedule, szZoneId, 0);
						EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, m_szResponse, -1);

						if (!m_szResponse.find("\"id\""))
							continue;

						Json::Value jDailySchedule;
						if (evohome::parse_json_string(m_szResponse, jDailySchedule) < 0)
						{
							m_szLastError = "Failed to parse server response as JSON";
							continue;
						}

						Json::Value jBackupScheduleZone;
						jBackupScheduleZone["zoneId"] = szZoneId;
						jBackupScheduleZone["name"] = (*jTCS)["zones"][iz]["name"].asString();
						if (jDailySchedule["dailySchedules"].isArray())
							jBackupScheduleZone["dailySchedules"] = jDailySchedule["dailySchedules"];
						else
							jBackupScheduleZone["dailySchedules"] = Json::arrayValue;
						jBackupScheduleTCS[szZoneId] = jBackupScheduleZone;
					}

					// Hot Water
					if (has_dhw(il, igw, itcs))
					{
						std::string szHotWaterId = (*jTCS)["dhw"]["dhwId"].asString();
						if (szHotWaterId.empty())
							continue;

						std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::zoneSchedule, szHotWaterId, 1);
						EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, m_szResponse, -1);

						if ( ! m_szResponse.find("\"id\""))
							return false;

						Json::Value jDailySchedule;
						if (evohome::parse_json_string(m_szResponse, jDailySchedule) < 0)
						{
							m_szLastError = "Failed to parse server response as JSON";
							continue;
						}

						Json::Value jBackupScheduleDHW;
						jBackupScheduleDHW["dhwId"] = szHotWaterId;
						if (jDailySchedule["dailySchedules"].isArray())
							jBackupScheduleDHW["dailySchedules"] = jDailySchedule["dailySchedules"];
						else
							jBackupScheduleDHW["dailySchedules"] = Json::arrayValue;
						jBackupScheduleTCS[szHotWaterId] = jBackupScheduleDHW;
					}
					jBackupScheduleGateway[szTCSId] = jBackupScheduleTCS;
				}
				jBackupScheduleLocation[szGatewayId] = jBackupScheduleGateway;
			}
			jBackupSchedule[szLocationId] = jBackupScheduleLocation;
		}

		myfile << jBackupSchedule.toStyledString() << "\n";
		myfile.close();
		return true;
	}
	return false;
}


/*
 * Load all schedules from a schedule backup file
 */
bool EvohomeClient2::load_schedules_from_file(const std::string &szFilename)
{
	std::string szFileContent;
	std::ifstream myfile (szFilename.c_str());
	if ( myfile.is_open() )
	{
		std::string line;
		while ( getline (myfile,line) )
		{
			szFileContent.append(line);
			szFileContent.append("\n");
		}
		myfile.close();
	}
	if (szFileContent == "")
		return false;

	Json::Value jSchedule;

	if (evohome::parse_json_string(szFileContent, jSchedule) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	Json::Value::Members locations = jSchedule.getMemberNames();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		if (jSchedule[locations[il]].isString())
			continue;
		Json::Value::Members gateways = jSchedule[locations[il]].getMemberNames();

		int numGateways = static_cast<int>(gateways.size());
		for (int igw = 0; igw < numGateways; igw++)
		{
			if (jSchedule[locations[il]][gateways[igw]].isString())
				continue;

			Json::Value *jGateway = &jSchedule[locations[il]][gateways[igw]];
			Json::Value::Members temperatureControlSystems = (*jGateway).getMemberNames();

			int numTCS = static_cast<int>(temperatureControlSystems.size());
			for (int itcs = 0; itcs < numTCS; itcs++)
			{
				if ((*jGateway)[temperatureControlSystems[itcs]].isString())
					continue;

				Json::Value *jTCS = &(*jGateway)[temperatureControlSystems[itcs]];
				Json::Value::Members zones = (*jTCS).getMemberNames();

				int numZones = static_cast<int>(zones.size());
				for (int iz = 0; iz < numZones; iz++)
				{
					if ((*jTCS)[zones[iz]].isString())
						continue;
					evohome::device::zone *zone = get_zone_by_ID(zones[iz]);
					if (zone != NULL)
						zone->jSchedule = (*jTCS)[zones[iz]];
				}
			}
		}
	}
	return true;
}


/*
 * Set a zone's schedule
 */
bool EvohomeClient2::set_zone_schedule(const std::string szZoneId, Json::Value *jSchedule)
{
	return set_zone_schedule_ex(szZoneId, 0, jSchedule);
}
bool EvohomeClient2::set_dhw_schedule(const std::string szDHWId, Json::Value *jSchedule)
{
	return set_zone_schedule_ex(szDHWId, 1, jSchedule);
}
/* private */ bool EvohomeClient2::set_zone_schedule_ex(const std::string szZoneId, const unsigned int zoneType, Json::Value *jSchedule)
{
	std::string szPutdata = (*jSchedule).toStyledString();
	int numChars = static_cast<int>(szPutdata.length());
	for (int i = 0; i < numChars; i++)
	{
		char c = szPutdata[i];
		if (c == '"')
		{
			i++;
			c = szPutdata[i];
			if ((c > 0x60) && (c < 0x7b))
				szPutdata[i] = c ^ 0x20;
		}
	}

	size_t insertAt = 0;
	while ((insertAt = szPutdata.find("\"Temperature\"")) != std::string::npos)
	{
		insertAt++;
		szPutdata.insert(insertAt, "Target");
		insertAt +=17;
	}

	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::zoneSchedule, szZoneId, zoneType);
	EvoHTTPBridge::SafePUT(szUrl, szPutdata, m_vEvoHeader, m_szResponse, -1);

	if (m_szResponse.find("\"id\""))
		return true;
	return false;
}


/*
 * Restore all schedules from a backup file
 */
bool EvohomeClient2::schedules_restore(const std::string &szFilename)
{
	if ( ! load_schedules_from_file(szFilename) )
		return false;

	std::cout << "Restoring schedules from file " << szFilename << "\n";

	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		std::cout << "  Location: " << m_vLocations[il].szLocationId << "\n";

		int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
		for (int igw = 0; igw < numGateways; igw++)
		{
			evohome::device::gateway *gw = &m_vLocations[il].gateways[igw];
			std::cout << "    Gateway: " << (*gw).szGatewayId << "\n";

			int numTCS = static_cast<int>((*gw).temperatureControlSystems.size());
			for (int itcs = 0; itcs < numTCS; itcs++)
			{
				evohome::device::temperatureControlSystem *tcs = &(*gw).temperatureControlSystems[itcs];
				std::cout << "      System: " << (*tcs).szSystemId << "\n";

				int numZones = static_cast<int>((*tcs).zones.size());
				for (int iz = 0; iz < numZones; iz++)
				{
					evohome::device::zone *zone = &(*tcs).zones[iz];
					std::cout << "        Zone: " << (*(*zone).jInstallationInfo)["name"].asString() << "\n";
					set_zone_schedule((*zone).szZoneId, &(*zone).jSchedule);
				}
				if (has_dhw(tcs))
				{
					std::string dhwId = (*(*tcs).jStatus)["dhw"]["dhwId"].asString();
					std::cout << "        Hot water\n";
					set_dhw_schedule(dhwId, &(*tcs).dhw[0].jSchedule);
				}
			}
		}
	}
	return true;
}


/************************************************************************
 *									*
 *	Evohome overrides						*
 *									*
 ************************************************************************/


/*
 * Set the system mode
 */
bool EvohomeClient2::set_system_mode(const std::string szSystemId, const std::string szMode, const std::string szDateUntil)
{
	int i = 0;
	int s = static_cast<int>(sizeof(evohome::API2::system::mode));
	while (s > 0)
	{
		if (evohome::API2::system::mode[i] == szMode)
			return set_system_mode(szSystemId, i, szDateUntil);
		s -= static_cast<int>(sizeof(evohome::API2::system::mode[i]));
		i++;
	}
	return false;
}
bool EvohomeClient2::set_system_mode(const std::string szSystemId, const unsigned int mode, const std::string szDateUntil)
{
	std::string szPutData = "{\"SystemMode\":\"";
	szPutData.append(evohome::API2::system::mode[mode]);
	szPutData.append("\",\"TimeUntil\":");
	if (szDateUntil.empty())
		szPutData.append("null");
	else if (!IsoTimeString::verify_date(szDateUntil))
		return false;
	else
	{
		std::string szTimeUntil = szDateUntil + "T00:00:00";
		szPutData.append("\"");
		szPutData.append(IsoTimeString::local_to_utc(szTimeUntil));
		szPutData.append("\"");
	}
	szPutData.append(",\"Permanent\":");
	if (szDateUntil.empty())
		szPutData.append("true}");
	else
		szPutData.append("false}");

	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::systemMode, szSystemId);
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, m_szResponse, -1);

	if (m_szResponse.find("\"id\""))
		return true;
	return false;
}


/*
 * Override a zone's target temperature
 */
bool EvohomeClient2::set_temperature(const std::string szZoneId, const std::string temperature, const std::string szTimeUntil)
{
	std::string szPutData = "{\"HeatSetpointValue\":";
	szPutData.append(temperature);
	szPutData.append(",\"SetpointMode\":\"");
	if (szTimeUntil.empty())
		szPutData.append(evohome::API2::zone::mode[1]);
	else if (!IsoTimeString::verify_datetime(szTimeUntil))
		return false;
	else
		szPutData.append(evohome::API2::zone::mode[2]);
	szPutData.append("\",\"TimeUntil\":");
	if (szTimeUntil.empty())
		szPutData.append("null}");
	else
	{
		szPutData.append("\"");
		szPutData.append(szTimeUntil.substr(0,10));
		szPutData.append("T");
		szPutData.append(szTimeUntil.substr(11,8));
		szPutData.append("Z\"}");
	}

	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::zoneSetpoint, szZoneId);
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, m_szResponse, -1);

	if (m_szResponse.find("\"id\""))
		return true;
	return false;
}


/*
 * Cancel a zone's target temperature override
 */
bool EvohomeClient2::cancel_temperature_override(const std::string szZoneId)
{
	std::string szPutData = "{\"HeatSetpointValue\":0.0,\"SetpointMode\":\"FollowSchedule\",\"TimeUntil\":null}";

	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::zoneSetpoint, szZoneId);
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, m_szResponse, -1);

	if (m_szResponse.find("\"id\""))
		return true;
	return false;
}


bool EvohomeClient2::has_dhw(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx)
{
	if (!verify_object_path(locationIdx, gatewayIdx, systemIdx))
		return false;
	return has_dhw(&m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[systemIdx]);
}
bool EvohomeClient2::has_dhw(const std::string szSystemId)
{
	return has_dhw(get_temperatureControlSystem_by_ID(szSystemId));
}
bool EvohomeClient2::has_dhw(evohome::device::temperatureControlSystem *tcs)
{
	if (tcs == NULL)
		return false;
	return ((*tcs).dhw.size() > 0);
}


bool EvohomeClient2::is_single_heating_system()
{
	if (m_vLocations.size() == 0)
		full_installation();
	return ( (m_vLocations.size() == 1) &&
		 (m_vLocations[0].gateways.size() == 1) &&
		 (m_vLocations[0].gateways[0].temperatureControlSystems.size() == 1) );
}


/*
 * Set mode for Hot Water device
 */
bool EvohomeClient2::set_dhw_mode(const std::string szDHWId, const std::string szMode,const  std::string szTimeUntil)
{
	std::string szPutData = "{\"State\":\"";
	if (szMode == "on")
		szPutData.append(evohome::API2::dhw::state[1]);
	else if (szMode == "off")
		szPutData.append(evohome::API2::dhw::state[0]);
	szPutData.append("\",\"Mode\":\"");
	if (szMode == "auto")
		szPutData.append(evohome::API2::zone::mode[0]);
	else if (szTimeUntil.empty())
		szPutData.append(evohome::API2::zone::mode[1]);
	else if (!IsoTimeString::verify_datetime(szTimeUntil))
		return false;
	else
		szPutData.append(evohome::API2::zone::mode[2]);
	szPutData.append("\",\"UntilTime\":");
	if (szTimeUntil.empty())
		szPutData.append("null}");
	else
	{
		szPutData.append("\"");
		szPutData.append(szTimeUntil.substr(0,10));
		szPutData.append("T");
		szPutData.append(szTimeUntil.substr(11,8));
		szPutData.append("Z\"}");
	}

	std::string szUrl = evohome::API2::uri::get_uri(evohome::API2::uri::dhwState, szDHWId);
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, m_szResponse, -1);

	if (m_szResponse.find("\"id\""))
		return true;
	return false;
}



/************************************************************************
 *									*
 *	Return Data Fields						*
 *									*
 ************************************************************************/

std::string EvohomeClient2::get_zone_temperature(const std::string szZoneId)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_temperature(myZone);
}
std::string EvohomeClient2::get_zone_temperature(const evohome::device::zone *zone)
{
	Json::Value *jZoneStatus = (*zone).jInstallationInfo;
	return (*jZoneStatus)["temperatureStatus"]["temperature"].asString();
}


std::string EvohomeClient2::get_zone_setpoint(const std::string szZoneId)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_setpoint(myZone);
}
std::string EvohomeClient2::get_zone_setpoint(const evohome::device::zone *zone)
{
	Json::Value *jZoneStatus = (*zone).jStatus;
	return (*jZoneStatus)["setpointStatus"]["targetHeatTemperature"].asString();
}


std::string EvohomeClient2::get_zone_mode(const std::string szZoneId)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_mode(myZone);
}
std::string EvohomeClient2::get_zone_mode(const evohome::device::zone *zone)
{
	Json::Value *jZoneStatus = (*zone).jStatus;
	return (*jZoneStatus)["setpointStatus"]["setpointMode"].asString();
}


std::string EvohomeClient2::get_zone_mode_until(const std::string szZoneId, const bool bLocaltime)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_mode_until(myZone);
}
std::string EvohomeClient2::get_zone_mode_until(const evohome::device::zone *zone, const bool bLocaltime)
{
	Json::Value *jZoneSetpoint = &(*zone->jStatus)["setpointStatus"];
	std::string szResult;
	if ((*jZoneSetpoint).isMember("until"))
		szResult = (*jZoneSetpoint)["until"].asString();
	if (szResult.size() < 10)
		return m_szEmptyFieldResponse;
	if (!bLocaltime)
		return szResult;
	return IsoTimeString::utc_to_local(szResult);
}


std::string EvohomeClient2::get_zone_name(const std::string szZoneId)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_name(myZone);
}
std::string EvohomeClient2::get_zone_name(const evohome::device::zone *zone)
{
	Json::Value *jZoneInfo = (*zone).jInstallationInfo;
	return (*jZoneInfo)["name"].asString();
}


std::string EvohomeClient2::get_location_name(const std::string szLocationId)
{
	int iloc = get_location_index(szLocationId);
	if (iloc < 0)
		return m_szEmptyFieldResponse;

	return get_location_name(iloc);
}
std::string EvohomeClient2::get_location_name(const unsigned int locationIdx)
{
	if (!verify_object_path(locationIdx))
		return m_szEmptyFieldResponse;

	Json::Value *jLocation = m_vLocations[locationIdx].jInstallationInfo;
	return (*jLocation)["name"].asString();
}


std::string EvohomeClient2::get_system_mode(const std::string szSystemId)
{
	evohome::device::temperatureControlSystem *myTCS = get_temperatureControlSystem_by_ID(szSystemId);
	if (myTCS == NULL)
		return m_szEmptyFieldResponse;

	return get_system_mode(myTCS);
}
std::string EvohomeClient2::get_system_mode(const evohome::device::temperatureControlSystem *tcs)
{
	Json::Value *jSystemMode = &(*tcs->jStatus)["systemModeStatus"];
	return (*jSystemMode)["mode"].asString();
}


std::string EvohomeClient2::get_system_mode_until(const std::string szSystemId, const bool bLocaltime)
{
	evohome::device::temperatureControlSystem *myTCS = get_temperatureControlSystem_by_ID(szSystemId);
	if (myTCS == NULL)
		return m_szEmptyFieldResponse;

	return get_system_mode_until(myTCS);
}
std::string EvohomeClient2::get_system_mode_until(const evohome::device::temperatureControlSystem *tcs, const bool bLocaltime)
{
	Json::Value *jSystemMode = &(*tcs->jStatus)["systemModeStatus"];
	std::string szResult;
	if ((*jSystemMode).isMember("timeUntil"))
		szResult = (*jSystemMode)["timeUntil"].asString();
	if (szResult.size() < 10)
		return m_szEmptyFieldResponse;
	if (!bLocaltime)
		return szResult;
	return IsoTimeString::utc_to_local(szResult);
}


/************************************************************************
 *									*
 *	Sanity checks							*
 *									*
 ************************************************************************/


/* 
 * Passing an ID beyond a vector's size will cause a segfault
 */

/* private */ bool EvohomeClient2::verify_object_path(const unsigned int locationIdx)
{
	return ( (locationIdx < static_cast<unsigned int>(m_vLocations.size())) );
}
/* private */ bool EvohomeClient2::verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx)
{
	return ( (locationIdx < static_cast<unsigned int>(m_vLocations.size())) &&
		 (gatewayIdx < static_cast<unsigned int>(m_vLocations[locationIdx].gateways.size()))
	       );
}
/* private */ bool EvohomeClient2::verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx)
{
	return ( (locationIdx < static_cast<unsigned int>(m_vLocations.size())) &&
		 (gatewayIdx < static_cast<unsigned int>(m_vLocations[locationIdx].gateways.size())) &&
		 (systemIdx < static_cast<unsigned int>(m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems.size()))
	       );
}
/* private */ bool EvohomeClient2::verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx, const unsigned int zoneIdx)
{
	return ( (locationIdx < static_cast<unsigned int>(m_vLocations.size())) &&
		 (gatewayIdx < static_cast<unsigned int>(m_vLocations[locationIdx].gateways.size())) &&
		 (systemIdx < static_cast<unsigned int>(m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems.size())) &&
		 (zoneIdx < static_cast<unsigned int>(m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[systemIdx].zones.size()))
	       );
}

