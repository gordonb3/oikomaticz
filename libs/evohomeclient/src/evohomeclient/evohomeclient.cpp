/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for 'Old' US Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>

#include "evohomeclient.hpp"
#include "../connection/EvoHTTPBridge.hpp"
#include "../evohome/jsoncppbridge.hpp"
#include "../time/IsoTimeString.hpp"



#define EVOHOME_HOST "https://tccna.honeywell.com"

#define SESSION_EXPIRATION_TIME 899


namespace evohome {

  namespace API {

    namespace auth {
	static const std::string applicationId = "91db1612-73fd-4500-91b2-e63b069b185c";
    }; // namespace auth

    namespace header {
      static const std::string accept = "Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml";
      static const std::string jsondata = "Content-Type: application/json";

    }; // namespace header

    namespace system {
      static const std::string mode[7] = {"Auto", "HeatingOff", "AutoWithEco", "Away", "DayOff", "", "Custom"};
    }; // namespace system

    namespace device {
      static const std::string mode[7] = {"Scheduled", "Hold", "Temporary", "", "", "", ""};
      static const std::string type[2] = {"EMEA_ZONE", "DOMESTIC_HOT_WATER"};
      static const std::string state[2] = {"DHWOff", "DHWOn"};
    }; // namespace device

    namespace uri {
      static const std::string base = EVOHOME_HOST"/WebAPI/api/";

      static const std::string login = "Session";
      static const std::string installationInfo = "locations/?allData=True&userId={id}";
      static const std::string systemMode = "evoTouchSystems?locationIdx={id}";
      static const std::string deviceMode = "devices/{id}/thermostat/changeableValues";
      static const std::string deviceSetpoint = "/heatSetpoint";

      static std::string get_uri(const std::string &szApiFunction, const std::string &szId = "", const uint8_t zoneType = 0)
      {
        std::string result = szApiFunction;

        if (szApiFunction == login)
        {
          // all done
        }
        else if (szApiFunction == installationInfo)
          result.replace(31, 4, szId);
        else if (szApiFunction == systemMode)
          result.replace(27, 4, szId);
        else if (szApiFunction == deviceMode)
          result.replace(8, 4, szId);
        else if (szApiFunction == deviceSetpoint)
        {
          result = deviceMode;
          result.replace(8, 4, szId);
          result.append(szApiFunction);
        }
        else
          return ""; // invalid input

        return result.insert(0, evohome::API::uri::base);
      }


    }; // namespace uri

  }; // namespace API

}; // namespace evohome


/*
 * Class construct
 */
EvohomeClient::EvohomeClient()
{
	init();
}

EvohomeClient::~EvohomeClient()
{
	cleanup();
}


/************************************************************************
 *									*
 *	Helpers								*
 *									*
 ************************************************************************/


/* private */ void EvohomeClient::init()
{
	m_szEmptyFieldResponse = "<null>";
}


/*
 * Cleanup curl web client
 */
void EvohomeClient::cleanup()
{
	EvoHTTPBridge::CloseConnection();
}


std::string EvohomeClient::get_last_error()
{
	return m_szLastError;
}


std::string EvohomeClient::get_last_response()
{
	return m_szResponse;
}


void EvohomeClient::set_empty_field_response(std::string szResponse)
{
	m_szEmptyFieldResponse = szResponse;
}

/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 ************************************************************************/


/* 
 * login to evohome web server
 */
bool EvohomeClient::login(const std::string &szUsername, const std::string &szPassword)
{
	std::vector<std::string> vLoginHeader;
	vLoginHeader.push_back(evohome::API::header::accept);
	vLoginHeader.push_back(evohome::API::header::jsondata);

	std::string szPostdata = "{'Username': '{usr}', 'Password': '{pwd}', 'ApplicationId': '{app}'}";
	szPostdata.replace(61, 5, evohome::API::auth::applicationId);
	szPostdata.replace(35, 5, EvoHTTPBridge::URLEncode(szPassword));
	szPostdata.replace(14, 5, szUsername);

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::login);
	EvoHTTPBridge::SafePOST(szUrl, szPostdata, vLoginHeader, m_szResponse, -1);
	m_tLastWebCall = time(NULL);

	Json::Value jLogin;
	if (evohome::parse_json_string(m_szResponse, jLogin) < 0)
		return false;

	std::string szError = "";
	if (jLogin.isMember("error"))
		szError = jLogin["error"].asString();
	if (jLogin.isMember("message"))
		szError = jLogin["message"].asString();
	if (!szError.empty())
	{
		m_szLastError = "Login to Evohome server failed with message: ";
		m_szLastError.append(szError);
		return false;
	}

	if (!jLogin.isMember("sessionId") || !jLogin.isMember("userInfo") || !jLogin["userInfo"].isObject() || !jLogin["userInfo"].isMember("userID"))
	{
		m_szLastError = "Login to Evohome server did not return required data";
		return false;
	}

	m_szSessionId = jLogin["sessionId"].asString();
	m_szUserId = jLogin["userInfo"]["userID"].asString();

	std::string szAuthBearer = "SessionID: ";
	szAuthBearer.append(m_szSessionId);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back(evohome::API::header::accept);

	return true;
}


/*
 * Save authorization key to a backup file
 */
bool EvohomeClient::save_auth_to_file(const std::string &szFilename)
{
	std::ofstream myfile (szFilename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value jAuth;

		jAuth["session_id"] = m_szSessionId;
		jAuth["last_use"] = static_cast<int>(m_tLastWebCall);
		jAuth["user_id"] = m_szUserId;

		myfile << jAuth.toStyledString() << "\n";
		myfile.close();
		return true;
	}
	return false;
}


/*
 * Load authorization key from a backup file
 */
bool EvohomeClient::load_auth_from_file(const std::string &szFilename)
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
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szFileContent.c_str(), szFileContent.c_str() + szFileContent.size(), &jAuth, nullptr))
		return false;

	m_szSessionId = jAuth["session_id"].asString();
	m_tLastWebCall = static_cast<time_t>(atoi(jAuth["last_use"].asString().c_str()));
	m_szUserId = jAuth["user_id"].asString();

	if ((time(NULL) - m_tLastWebCall) > SESSION_EXPIRATION_TIME)
		return false;

	std::string szAuthBearer = "SessionID: ";
	szAuthBearer.append(m_szSessionId);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back(evohome::API::header::accept);

	return is_session_valid();
}


bool EvohomeClient::is_session_valid()
{
	if (m_szUserId.empty())
		return false;

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::login);
	EvoHTTPBridge::SafePUT(szUrl, "", m_vEvoHeader, m_szResponse, -1);
	m_tLastWebCall = time(NULL);

	Json::Value jSession;
	if (evohome::parse_json_string(m_szResponse, jSession) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	if (jSession.isMember("code") && (jSession["code"].asString() != "-1") && (jSession["code"].asString() != "204"))
	{
		// session is no longer valid
		m_szUserId = "";
		return false;
	}
	return true;
}





/************************************************************************
 *									*
 *	Evohome heating installations					*
 *									*
 ************************************************************************/


/* private */ void EvohomeClient::get_devices(const unsigned int locationIdx, const unsigned int gatewayIdx)
{
	evohome::device::temperatureControlSystem *myTCS = &m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[0];
	std::vector<evohome::device::zone>().swap((*myTCS).zones);
	std::vector<evohome::device::zone>().swap((*myTCS).dhw);

	Json::Value *jLocation = m_vLocations[locationIdx].jInstallationInfo;
	if (!(*jLocation)["devices"].isArray())
		return;

	int l = static_cast<int>((*jLocation)["devices"].size());
	int zoneIdx = 0;
	for (int i = 0; i < l; ++i)
	{
		if ((*jLocation)["devices"][i]["gatewayId"].asString() == m_vLocations[locationIdx].gateways[gatewayIdx].szGatewayId)
		{
			evohome::device::zone newDevice = evohome::device::zone();
			evohome::device::path::zone newzonepath = evohome::device::path::zone();
			newDevice.jInstallationInfo = &(*jLocation)["devices"][i];
			newDevice.szZoneId = (*jLocation)["devices"][i]["deviceID"].asString();
			newDevice.zoneIdx = zoneIdx;
			newDevice.systemIdx = 0;
			newDevice.gatewayIdx = gatewayIdx;
			newDevice.locationIdx = locationIdx;
			if ((*jLocation)["devices"][i]["thermostatModelType"].asString() == evohome::API::device::type[0])
			{
				(*myTCS).zones.push_back(newDevice);
				newzonepath.zoneIdx = zoneIdx;
			}
			else if ((*jLocation)["devices"][i]["thermostatModelType"].asString() == evohome::API::device::type[1])
			{
				(*myTCS).dhw.push_back(newDevice);
				newzonepath.zoneIdx = 128;
			}

			newzonepath.locationIdx = locationIdx;
			newzonepath.gatewayIdx = gatewayIdx;
			newzonepath.systemIdx = 0;
			newzonepath.szZoneId = newDevice.szZoneId;
			m_vZonePaths.push_back(newzonepath);

			zoneIdx++;
		}
	}
}


/* private */ void EvohomeClient::get_temperatureControlSystems(const unsigned int locationIdx, const unsigned int gatewayIdx)
{
	evohome::device::gateway *myGateway = &m_vLocations[locationIdx].gateways[gatewayIdx];
	std::vector<evohome::device::temperatureControlSystem>().swap((*myGateway).temperatureControlSystems);

	(*myGateway).temperatureControlSystems.resize(1);
	(*myGateway).temperatureControlSystems[0].systemIdx = 0;
	(*myGateway).temperatureControlSystems[0].gatewayIdx = gatewayIdx;
	(*myGateway).temperatureControlSystems[0].locationIdx = locationIdx;

	get_devices(locationIdx, gatewayIdx);
}


/* private */ void EvohomeClient::get_gateways(const unsigned int locationIdx)
{
	std::vector<evohome::device::gateway>().swap(m_vLocations[locationIdx].gateways);
	Json::Value *jLocation = m_vLocations[locationIdx].jInstallationInfo;

	if (!(*jLocation)["devices"].isArray())
		return;

	int l = static_cast<int>((*jLocation)["devices"].size());
	std::string szGatewayID;
	int gatewayIdx = 0;
	for (int i = 0; i < l; ++i)
	{
		std::string szDeviceGatewayID = (*jLocation)["devices"][i]["gatewayId"].asString();
		if (szDeviceGatewayID != szGatewayID)
		{
			szGatewayID = szDeviceGatewayID;
			bool bNewgateway = true;
			for (int j = 0; j < i; ++j)
			{
				if (m_vLocations[locationIdx].gateways[i].szGatewayId == szGatewayID)
				{
					bNewgateway = false;
					j = i;
				}
			}

			if (bNewgateway)
			{
				evohome::device::gateway newGateway = evohome::device::gateway();
				newGateway.szGatewayId = szGatewayID;
				newGateway.gatewayIdx = gatewayIdx;
				newGateway.locationIdx = locationIdx;
				m_vLocations[locationIdx].gateways.push_back(newGateway);

				get_temperatureControlSystems(locationIdx, gatewayIdx);
				gatewayIdx++;
			}
		}
	}
}


/* 
 * Retrieve evohome installation info
 */
bool EvohomeClient::full_installation()
{
	std::vector<evohome::device::location>().swap(m_vLocations);
	std::vector<evohome::device::path::zone>().swap(m_vZonePaths);

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::installationInfo, m_szUserId);
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, m_szResponse, -1);
	m_tLastWebCall = time(NULL);

	// evohome old API returns an unnamed json array which is not accepted by our parser
	m_szResponse.insert(0, "{\"locations\": ");
	m_szResponse.append("}");

	m_jFullInstallation.clear();
	if (evohome::parse_json_string(m_szResponse, m_jFullInstallation) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	if (!m_jFullInstallation["locations"].isArray())
		return false;

	if (!m_jFullInstallation["locations"][0].isMember("locationID"))
		return false;

	int l = static_cast<int>(m_jFullInstallation["locations"].size());
	for (int i = 0; i < l; ++i)
	{
		evohome::device::location newloc = evohome::device::location();
		m_vLocations.push_back(newloc);
		m_vLocations[i].jInstallationInfo = &m_jFullInstallation["locations"][i];
		m_vLocations[i].szLocationId = (*m_vLocations[i].jInstallationInfo)["locationID"].asString();
		m_vLocations[i].locationIdx = i;

		get_gateways(i);
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
bool EvohomeClient::set_system_mode(const std::string szLocationId, const std::string szMode, const std::string szDateUntil)
{
	int i = 0;
	int s = static_cast<int>(sizeof(evohome::API::system::mode));
	while (s > 0)
	{
		if (evohome::API::system::mode[i] == szMode)
			return set_system_mode(szLocationId, i, szDateUntil);
		s -= static_cast<int>(sizeof(evohome::API::system::mode[i]));
		i++;
	}
	return false;
}
bool EvohomeClient::set_system_mode(const std::string szLocationId, const unsigned int mode, const std::string szDateUntil)
{
	std::string szPutData = "{\"QuickAction\":\"";
	szPutData.append(evohome::API::system::mode[mode]);
	szPutData.append("\",\"QuickActionNextTime\":");
	if (szDateUntil.empty())
		szPutData.append("null}");
	else if (!IsoTimeString::verify_date(szDateUntil))
		return false;
	else
	{
		szPutData.append("\"");
		szPutData.append(szDateUntil.substr(0,10));
		szPutData.append("T00:00:00Z\"}");
	}

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::systemMode, szLocationId);
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, m_szResponse, -1);

	if (m_szResponse.find("\"id\""))
		return true;
	return false;
}


/*
 * Override a zone's target temperature
 */
bool EvohomeClient::set_temperature(const std::string szZoneId, const std::string temperature, const std::string szTimeUntil)
{
	std::string szPutData = "{\"Value\":";
	szPutData.append(temperature);
	szPutData.append(",\"Status\":\"");
	if (szTimeUntil.empty())
		szPutData.append(evohome::API::device::mode[1]);
	else if (!IsoTimeString::verify_datetime(szTimeUntil))
		return false;
	else
		szPutData.append(evohome::API::device::mode[2]);
	szPutData.append("\",\"NextTime\":");
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

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::deviceSetpoint, szZoneId);
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, m_szResponse, -1);

	if (m_szResponse.find("\"id\""))
		return true;
	return false;
}


/*
 * Cancel a zone's target temperature override
 */
bool EvohomeClient::cancel_temperature_override(const std::string szZoneId)
{
	std::string szPutData = "{\"Value\":null,\"Status\":\"Scheduled\",\"NextTime\":null}";

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::deviceSetpoint, szZoneId);
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, m_szResponse, -1);

	if (m_szResponse.find("\"id\""))
		return true;
	return false;
}


bool EvohomeClient::has_dhw(const unsigned int locationIdx, const unsigned int gatewayIdx)
{
	if (!verify_object_path(locationIdx, gatewayIdx))
		return false;

	evohome::device::temperatureControlSystem *myTCS = &m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[0];
	return ((*myTCS).dhw.size() > 0);
}


bool EvohomeClient::is_single_heating_system()
{
	if (m_vLocations.size() == 0)
		full_installation();
	return ( (m_vLocations.size() == 1) && (m_vLocations[0].gateways.size() == 1) );
}


/*
 * Set mode for Hot Water device
 */
bool EvohomeClient::set_dhw_mode(const std::string szDHWId, const std::string szMode, const std::string szTimeUntil)
{
	std::string szPutData = "{\"Status\":\"";
	if (szMode == "auto")
		szPutData.append(evohome::API::device::mode[0]);
	else if (szTimeUntil.empty())
		szPutData.append(evohome::API::device::mode[1]);
	else if (!IsoTimeString::verify_datetime(szTimeUntil))
		return false;
	else
		szPutData.append(evohome::API::device::mode[2]);

	szPutData.append("\",\"Mode\":");
	if (szMode == "auto")
		szPutData.append("null");
	else
	{
		szPutData.append("\"");
		if (szMode == "on")
			szPutData.append(evohome::API::device::state[1]);
		else // if (szMode == "off")
			szPutData.append(evohome::API::device::state[0]);
		szPutData.append("\"");
	}
	szPutData.append(",\"NextTime\":");
	if (szTimeUntil.empty())
		szPutData.append("null}");
	else
	{
		szPutData.append("\"");
		szPutData.append(szTimeUntil.substr(0,10));
		szPutData.append("T");
		szPutData.append(szTimeUntil.substr(11,8));
		szPutData.append("Z\"");
	}
	szPutData.append(",\"SpecialModes\": null,\"HeatSetpoint\": null,\"CoolSetpoint\": null}");

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::deviceMode, szDHWId);
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, m_szResponse, -1);

	if (m_szResponse.find("\"id\""))
		return true;
	return false;
}


bool EvohomeClient::cancel_dhw_override(const std::string szDHWId)
{
	return set_dhw_mode(szDHWId, "auto");
}




/************************************************************************
 *									*
 *	Locate Evohome elements						*
 *									*
 ************************************************************************/

int EvohomeClient::get_location_index(const std::string szLocationId)
{
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int iloc = 0; iloc < numLocations; iloc++)
	{
		if (m_vLocations[iloc].szLocationId == szLocationId)
			return iloc;
	}
	return -1;
}


int EvohomeClient::get_zone_index(const unsigned int locationIdx, const unsigned int gatewayIdx, const std::string szZoneId)
{
	if (!verify_object_path(locationIdx, gatewayIdx))
		return -1;

	evohome::device::temperatureControlSystem *myTCS = &m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[0];
	int numZones = static_cast<int>((*myTCS).zones.size());
	for (int iz = 0; iz < numZones; iz++)
	{
		if ((*myTCS).zones[iz].szZoneId == szZoneId)
			return iz;
	}
	return -1;

}


/* private */ evohome::device::path::zone *EvohomeClient::get_zone_path(const std::string szZoneId)
{
	int iz = get_zone_path_ID(szZoneId);
	if (iz < 0)
		return NULL;
	return &m_vZonePaths[iz];
}


/* private */ int EvohomeClient::get_zone_path_ID(const std::string szZoneId)
{
	int numZones = static_cast<int>(m_vZonePaths.size());
	for (int iz = 0; iz < numZones; iz++)
	{
		if (m_vZonePaths[iz].szZoneId == szZoneId)
			return iz;
	}
	return -1;
}


evohome::device::zone *EvohomeClient::get_zone_by_ID(const std::string szZoneId)
{
	evohome::device::path::zone *zp = get_zone_path(szZoneId);
	if (zp == NULL)
		return NULL;

	if (zp->zoneIdx & 128)
		return &m_vLocations[zp->locationIdx].gateways[zp->gatewayIdx].temperatureControlSystems[zp->systemIdx].dhw[0];
	else
		return &m_vLocations[zp->locationIdx].gateways[zp->gatewayIdx].temperatureControlSystems[zp->systemIdx].zones[zp->zoneIdx];
}


evohome::device::zone *EvohomeClient::get_zone_by_Name(std::string szZoneName)
{
	int numZones = static_cast<int>(m_vZonePaths.size());
	for (int iz = 0; iz < numZones; iz++)
	{
		evohome::device::path::zone *zp = &m_vZonePaths[iz];
		evohome::device::zone *myZone = &m_vLocations[zp->locationIdx].gateways[zp->gatewayIdx].temperatureControlSystems[0].zones[zp->zoneIdx];
		if (!(zp->zoneIdx & 128) && ((*myZone->jInstallationInfo)["name"].asString() == szZoneName))
			return myZone;
	}
	return NULL;
}


/************************************************************************
 *									*
 *	Sanity checks							*
 *									*
 ************************************************************************/

/* 
 * Passing an ID beyond a vector's size will cause a segfault
 */

/* private */ bool EvohomeClient::verify_object_path(const unsigned int locationIdx)
{
	return ( (locationIdx < static_cast<unsigned int>(m_vLocations.size())) );
}
/* private */ bool EvohomeClient::verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx)
{
	return ( (locationIdx < static_cast<unsigned int>(m_vLocations.size())) &&
		 (gatewayIdx < static_cast<unsigned int>(m_vLocations[locationIdx].gateways.size()))
	       );
}
/* private */ bool EvohomeClient::verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int zoneIdx)
{
	return ( (locationIdx < static_cast<unsigned int>(m_vLocations.size())) &&
		 (gatewayIdx < static_cast<unsigned int>(m_vLocations[locationIdx].gateways.size())) &&
		 (zoneIdx < static_cast<unsigned int>(m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[0].zones.size()))
	       );
}




/************************************************************************
 *									*
 *	Return Data Fields						*
 *									*
 ************************************************************************/


/* 
 * Extract a zone's temperature from system status
 */
std::string EvohomeClient::get_zone_temperature(const std::string szZoneId, const unsigned int numDecimals)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_temperature(myZone, numDecimals);
}
std::string EvohomeClient::get_zone_temperature(const evohome::device::zone *zone, const unsigned int numDecimals)
{
	Json::Value *jZoneThermostat = &(*zone->jInstallationInfo)["thermostat"];
	double temperature = (*jZoneThermostat)["indoorTemperature"].asDouble();
	if (temperature > 127) // allow rounding error
		return "128"; // unit is offline

	static char cTemperature[10];
	if (numDecimals < 2)
		sprintf(cTemperature, "%.01f", ((floor((temperature * 10) + 0.5) / 10) + 0.0001));
	else			
		sprintf(cTemperature, "%.02f", ((floor((temperature * 100) + 0.5) / 100) + 0.0001));
	return std::string(cTemperature);
}


std::string EvohomeClient::get_zone_setpoint(const std::string szZoneId)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_setpoint(myZone);
}
std::string EvohomeClient::get_zone_setpoint(const evohome::device::zone *zone)
{
	Json::Value *jZoneSetpoint = &(*zone->jInstallationInfo)["thermostat"]["changeableValues"]["heatSetpoint"];
	return (*jZoneSetpoint)["value"].asString();
}


std::string EvohomeClient::get_zone_mode(const std::string szZoneId)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_mode(myZone);
}
std::string EvohomeClient::get_zone_mode(const evohome::device::zone *zone)
{
	Json::Value *jZoneChangeable = &(*zone->jInstallationInfo)["thermostat"]["changeableValues"];
	if ((*zone).zoneIdx & 128) // DHW
		return (*jZoneChangeable)["status"].asString();
	return (*jZoneChangeable)["heatSetpoint"]["status"].asString();
}


std::string EvohomeClient::get_zone_mode_until(const std::string szZoneId)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_mode(myZone);
}
std::string EvohomeClient::get_zone_mode_until(const evohome::device::zone *zone)
{
	Json::Value *jZoneChangeable = &(*zone->jInstallationInfo)["thermostat"]["changeableValues"];
	if ((*zone).zoneIdx & 128) // DHW
	{
		if ((*jZoneChangeable).isMember("nextTime"))
			return (*jZoneChangeable)["nextTime"].asString();
		return m_szEmptyFieldResponse;
	}
	Json::Value *jZoneSetpoint = &(*jZoneChangeable)["heatSetpoint"];
	if ((*jZoneSetpoint).isMember("nextTime"))
		return (*jZoneSetpoint)["nextTime"].asString();
	return m_szEmptyFieldResponse;
}


std::string EvohomeClient::get_zone_name(const std::string szZoneId)
{
	evohome::device::zone *myZone = get_zone_by_ID(szZoneId);
	if (myZone == NULL)
		return m_szEmptyFieldResponse;

	return get_zone_name(myZone);
}
std::string EvohomeClient::get_zone_name(const evohome::device::zone *zone)
{
	Json::Value *jZoneInfo = (*zone).jInstallationInfo;
	return (*jZoneInfo)["name"].asString();
}


std::string EvohomeClient::get_location_name(const std::string szLocationId)
{
	int iloc = get_location_index(szLocationId);
	if (iloc < 0)
		return m_szEmptyFieldResponse;

	return get_location_name(iloc);
}
std::string EvohomeClient::get_location_name(const unsigned int locationIdx)
{
	if (!verify_object_path(locationIdx))
		return m_szEmptyFieldResponse;

	Json::Value *jLocation = m_vLocations[locationIdx].jInstallationInfo;
	return (*jLocation)["name"].asString();
}


/************************************************************************
 *									*
 *	obsolete methods - keep for backwards compatibility		*
 *									*
 ************************************************************************/


std::string EvohomeClient::get_zone_temperature(const std::string szLocationId, const std::string szZoneId, const unsigned int numDecimals)
{
	return get_zone_temperature(szZoneId, numDecimals);
}

