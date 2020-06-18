/*
 * Copyright (c) 2017 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Demo app for connecting to Evohome
 *
 */

#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <map>
#include <time.h>
#include "demo-defaults.hpp"
#include "evohomeclient/evohomeclient.hpp"
#include "evohomeclient2/evohomeclient2.hpp"


#define SHOW_DECIMALS 1


using namespace std;


time_t now;
int tzoffset=-1;
std::string lastzone = "";

std::string configfile;

bool verbose;

std::string ERROR = "ERROR: ";
std::string WARN = "WARNING: ";

bool highdef = true;


void exit_error(std::string message)
{
	cerr << message << endl;
	exit(1);
}


/*
 * Create an associative array with the zone information we need
 */
std::map<std::string,std::string> evo_get_zone_data(evohome::device::zone *zone)
{
	map<std::string,std::string> ret;
	ret["zoneId"] = (*zone->jStatus)["zoneId"].asString();
	ret["name"] = (*zone->jStatus)["name"].asString();
	ret["temperature"] = (*zone->jStatus)["temperatureStatus"]["temperature"].asString();
	ret["setpointMode"] = (*zone->jStatus)["setpointStatus"]["setpointMode"].asString();
	ret["targetTemperature"] = (*zone->jStatus)["setpointStatus"]["targetHeatTemperature"].asString();
	ret["until"] = (*zone->jStatus)["setpointStatus"]["until"].asString();
	return ret;
}


int main(int argc, char** argv)
{
// get current time
	now = time(NULL);

// get settings from config file
	configfile = CONF_FILE;
	read_evoconfig();

// connect to Evohome server
	std::cout << "connect to Evohome server\n";
	EvohomeClient2 *eclient;
	eclient = new EvohomeClient2();
	if (eclient->load_auth_from_file(AUTH_FILE_V2))
		std::cout << "    reusing saved connection (UK/EMEA)\n";
	else
	{
		if (eclient->login(evoconfig["usr"],evoconfig["pw"]))
		{
			std::cout << "    connected (UK/EMEA)\n";
			eclient->save_auth_to_file(AUTH_FILE_V2);
		}
		else
		{
			std::cout << "    login failed (UK/EMEA)\n";
			exit(1);
		}
	}

	EvohomeClient *v1client;
	v1client = new EvohomeClient();
	if (v1client->load_auth_from_file(AUTH_FILE_V1))
		std::cout << "    reusing saved connection (US)\n";
	else
	{
		if (v1client->login(evoconfig["usr"],evoconfig["pw"]))
		{
			std::cout << "    connected (US)\n";
		}
		else
		{
			std::cout << "    login failed (US)\n";
			highdef = false;
		}
	}

// what to return if there is no data
	eclient->set_empty_field_response("<null>");
	if (highdef)
	{
		v1client->set_empty_field_response("<null>");
	}

// retrieve Evohome installation
	std::cout << "retrieve Evohome installation\n";
	if (!eclient->full_installation())
	{
		std::cout << "    portal returned incorrect data\n";
		exit(1);
	}
	if (highdef)
	{
		highdef = v1client->full_installation();
	}

// set Evohome heating system
	int locationIdx = 0;
	int gatewayIdx = 0;
	int systemIdx = 0;

	if ( evoconfig.find("locationId") != evoconfig.end() )
	{
		while ( (eclient->m_vLocations[locationIdx].szLocationId != evoconfig["locationId"])  && (locationIdx < (int)eclient->m_vLocations.size()) )
			locationIdx++;
		if (locationIdx == (int)eclient->m_vLocations.size())
			exit_error(ERROR+"the Evohome location ID specified in "+CONF_FILE+" cannot be found");
	}
	if ( evoconfig.find("gatewayId") != evoconfig.end() )
	{
		while ( (eclient->m_vLocations[locationIdx].gateways[gatewayIdx].szGatewayId != evoconfig["gatewayId"])  && (gatewayIdx < (int)eclient->m_vLocations[locationIdx].gateways.size()) )
			gatewayIdx++;
		if (gatewayIdx == (int)eclient->m_vLocations[locationIdx].gateways.size())
			exit_error(ERROR+"the Evohome gateway ID specified in "+CONF_FILE+" cannot be found");
	}
	if ( evoconfig.find("systemId") != evoconfig.end() )
	{
		while ( (eclient->m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[systemIdx].szSystemId != evoconfig["systemId"])  && (systemIdx < (int)eclient->m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems.size()) )
			systemIdx++;
		if (systemIdx == (int)eclient->m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems.size())
			exit_error(ERROR+"the Evohome system ID specified in "+CONF_FILE+" cannot be found");
	}
	evohome::device::temperatureControlSystem *tcs = &eclient->m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[systemIdx];


// retrieve Evohome status
	std::cout << "retrieve Evohome status\n";
	if ( !	eclient->get_status(locationIdx) )
		std::cout << "status fail" << "\n";


// retrieving schedules and/or switchpoints can be slow because we can only fetch them for a single zone at a time.
// luckily schedules do not change very often, so we can use a local cache
	if ( ! eclient->load_schedules_from_file(SCHEDULE_CACHE) )
	{
		std::cout << "create local copy of schedules" << "\n";
		if ( ! eclient->schedules_backup(SCHEDULE_CACHE) )
			exit_error(ERROR+"failed to open schedule cache file '"+SCHEDULE_CACHE+"'");
		eclient->load_schedules_from_file(SCHEDULE_CACHE);
	}


// start demo output
	std::cout << "\nSystem info:\n";
	std::cout << "    Model Type = " << (*tcs->jInstallationInfo)["modelType"] << "\n";
	std::cout << "    System ID = " << (*tcs->jInstallationInfo)["systemId"] << "\n";
	std::cout << "    System mode = " << eclient->get_system_mode(tcs) << "\n";

	std::cout << "\nZones:\n";
	std::cout << "      ID       temp    ";
	if (highdef)
		std::cout << "v1temp      ";
	std::cout << "mode          setpoint      until               name\n";
	for (std::vector<evohome::device::zone>::size_type i = 0; i < tcs->zones.size(); ++i)
	{
		std::map<std::string,std::string> zone = evo_get_zone_data(&tcs->zones[i]);
		if (zone["until"].length() == 0)
		{
//			zone["until"] = eclient->request_next_switchpoint(zone["zoneId"]); // ask web portal (UTC)
			zone["until"] = eclient->get_next_switchpoint(zone["zoneId"]); // find in schedule (localtime)
//			zone["until"] = eclient->get_next_utcswitchpoint(zone["zoneId"]); // find in schedule (UTC)

			// get_next_switchpoint returns an assumed time zone indicator 'A' which only means to
			// differentiate from the UTC time zone indicator 'Z'. It's beyond the scope of this demo
			// and library to find the actual value for your timezone.
			if ((zone["until"].size() > 19) && (zone["until"][19] == 'A'))
				zone["until"] = zone["until"].substr(0,19);
		}
		else if (zone["until"].length() >= 19)
		{
			// Honeywell is mixing UTC and localtime in their returns
			// for display we need to convert overrides to localtime
			if (tzoffset == -1)
			{
				// calculate timezone offset once
				struct tm utime;
				gmtime_r(&now, &utime);
				tzoffset = difftime(mktime(&utime), now);
			}
			struct tm ltime;
			localtime_r(&now, &ltime);
			ltime.tm_isdst = -1;
			ltime.tm_hour = atoi(zone["until"].substr(11, 2).c_str());
			ltime.tm_min = atoi(zone["until"].substr(14, 2).c_str());
			ltime.tm_sec = atoi(zone["until"].substr(17, 2).c_str()) - tzoffset;
			mktime(&ltime);
			char until[40];
			sprintf(until,"%04d-%02d-%02dT%02d:%02d:%02d",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
			zone["until"] = string(until);
		}

		std::cout << "    " << zone["zoneId"];
//		std::cout << " => " << zone["temperature"];
		std::cout << " => " << eclient->get_zone_temperature(&tcs->zones[i]);
		if (highdef)
			std::cout << " => " << v1client->get_zone_temperature(zone["zoneId"], SHOW_DECIMALS);
		std::cout << " => " << zone["setpointMode"];
		std::cout << " => " << zone["targetTemperature"];
		std::cout << " => " << zone["until"];
		std::cout << " => " << zone["name"];
		std::cout << "\n";

		lastzone = zone["zoneId"];
	}

	std::cout << "\n";

// Dump json to screen
/*
	evohome::device::zone *myzone = eclient->get_zone_by_ID(lastzone);
	std::cout << "\nDump of installationinfo for zone" << lastzone << "\n";
	std::cout << (*myzone->jInstallationInfo).toStyledString() << "\n";
	std::cout << "\nDump of statusinfo for zone" << lastzone << "\n";
	std::cout << (*myzone->jStatus).toStyledString() << "\n";
*/


/*
	std::cout << "\nDump of full installationinfo\n";
	std::cout << eclient->m_vLocations[0].jInstallationInfo->toStyledString() << "\n";
	std::cout << "\nDump of full status\n";
	std::cout << eclient->m_vLocations[0].jStatus.toStyledString() << "\n";
*/


/*
	if (highdef)
	{
		std::cout << "\nDump of full installationinfo\n";
		std::cout << v1client->m_vLocations[0].jInstallationInfo->toStyledString() << "\n";
	}
*/


	if (highdef)
	{
		v1client->save_auth_to_file(AUTH_FILE_V1); // update the last use timestamp
		v1client->cleanup();
		delete v1client;
	}

	eclient->cleanup();
	delete eclient;

	return 0;
}

