/*
 * Copyright (c) 2016-2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for 'Old' US Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _EvohomeV1Client
#define _EvohomeV1Client

#include <vector>
#include <string>
#include "jsoncpp/json.h"

#include "../common/devices.hpp"


class EvohomeClient
{
public:
/************************************************************************
 *									*
 *	Main storage							*
 *									*
 ************************************************************************/

	std::vector<evohome::device::location> m_vLocations;

/************************************************************************
 *									*
 *	Debug information and errors 					*
 *									*
 ************************************************************************/

	std::string get_last_error();
	std::string get_last_response();


/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 *	Logins to the Evohome API stay valid indefinitely as long as	*
 *	you make your next call within 15 minutes after the previous	*
 *	one.								*
 *									*
 *	The Evohome portal sets a limit on the number of sessions that	*
 *	any given user can have. Thus if you are building a single run	*
 *	application you should use the provided save and load functions	*
 *	for the authentication tokens.					*
 *									*
 *	If load_auth_from_file() finds an expired authentication token  *
 *	it will return false and you need to login again.		*
 *									*
 *	UPDATE December 2019						*
 *	Automatic session renewal no longer works and the manual	*
 *	session renewal method that was added to the library to counter	*
 *	that issue also proved not to allow the session to live past 	*
 *	15 minutes from submitting credentials.	The added method has	*
 *	been renamed to is_session_valid() and I suggest you call it	*
 *	before attempting to retrieve any other data.			*
 *									*
 ************************************************************************/

	bool login(const std::string &szUsername, const std::string &szPassword);
	bool save_auth_to_file(const std::string &szFilename);
	bool load_auth_from_file(const std::string &szFilename);
	bool is_session_valid();

/************************************************************************
 *									*
 *	Evohome heating installations retrieval				*
 *									*
 *	full_installation() retrieves all the available information	*
 *	of your registered Evohome installation(s) and stores these	*
 *	in our internal structs. Because this information contains	*
 *	both the static and dynamic data, this will destroy existing	*
 *	structs and recreate them, thereby invalidating all pointers	*
 *	and references you may have towards a specific location or	*
 *	zone.								*
 *									*
 ************************************************************************/

	bool full_installation();


/************************************************************************
 *									*
 *	Evohome overrides						*
 *									*
 ************************************************************************/

	bool set_temperature(const std::string szZoneId, const std::string temperature, const std::string szTimeUntil = "");
	bool cancel_temperature_override(const std::string szZoneId);

	bool set_dhw_mode(const std::string szDHWId, const std::string szMode,const  std::string szTimeUntil = "");
	bool cancel_dhw_override(const std::string szDHWId);


/************************************************************************
 *									*
 *	Return Data Fields						*
 *									*
 ************************************************************************/

	std::string get_zone_temperature(const std::string szZoneId, const unsigned int numDecimals = 1);
	std::string get_zone_temperature(const evohome::device::zone *zone, const unsigned int numDecimals);

	std::string get_zone_setpoint(const std::string szZoneId);
	std::string get_zone_setpoint(const evohome::device::zone *zone);

	std::string get_zone_mode(const std::string szZoneId);
	std::string get_zone_mode(const evohome::device::zone *zone);

	std::string get_zone_mode_until(const std::string szZoneId);
	std::string get_zone_mode_until(const evohome::device::zone *zone);

	std::string get_zone_name(const std::string szZoneId);
	std::string get_zone_name(const evohome::device::zone *zone);

	std::string get_location_name(const std::string szLocationId);
	std::string get_location_name(const unsigned int locationIdx);


/************************************************************************
 *									*
 *	Simple tests							*
 *									*
 *	is_single_heating_system() returns true if your full		*
 *	installation contains just one temperature control system.	*
 *	When true, your locationIdx and gatewayIdx will both be 0	*
 *									*
 *	has_dhw returns true if the installation at the specified	*
 *	location contains a domestic hot water device.			*
 *									*
 ************************************************************************/

	bool is_single_heating_system();
	bool has_dhw(const unsigned int locationIdx, const unsigned int gatewayIdx = 0);


/************************************************************************
 *									*
 *	Object locators							*
 *									*
 *									*
 *									*
 ************************************************************************/

	int get_location_index(const std::string szLocationId);
	int get_zone_index(const unsigned int locationIdx, const unsigned int gatewayIdx, const std::string szZoneId);

	evohome::device::zone *get_zone_by_ID(const std::string szZoneId);
	evohome::device::zone *get_zone_by_Name(std::string szZoneName);


/************************************************************************
 *									*
 *	Class construct							*
 *									*
 ************************************************************************/

	EvohomeClient();
	~EvohomeClient();
	void cleanup();


/************************************************************************
 *									*
 *	Config options							*
 *									*
 ************************************************************************/

	void set_empty_field_response(std::string szResponse);


/************************************************************************
 *									*
 *	obsolete methods - keep for backwards compatibility		*
 *									*
 ************************************************************************/

	std::string get_zone_temperature(const std::string szLocationId, const std::string szZoneId, const unsigned int numDecimals = 1);





private:
	void init();

	void get_gateways(const unsigned int locationIdx);
	void get_temperatureControlSystems(const unsigned int locationIdx, const unsigned int gatewayIdx);
	void get_devices(const unsigned int locationIdx, const unsigned int gatewayIdx);

	bool verify_object_path(const unsigned int locationIdx);
	bool verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx);
	bool verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int zoneIdx);

	int get_zone_path_ID(const std::string szZoneId);
	evohome::device::path::zone *get_zone_path(const std::string szZoneId);

private:
	Json::Value m_jFullInstallation;
	std::string m_szUserId;
	std::string m_szSessionId;
	time_t m_tLastWebCall;
	std::vector<std::string> m_vEvoHeader;
	std::string m_szLastError;
	std::string m_szResponse;
	std::vector<evohome::device::path::zone> m_vZonePaths;

	std::string m_szEmptyFieldResponse;
};

#endif
