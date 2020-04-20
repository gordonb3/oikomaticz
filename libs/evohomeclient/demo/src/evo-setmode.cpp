/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Demo app for connecting to Evohome and Domoticz
 *
 *
 *
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <cstdlib>
#include "demo-defaults.hpp"
#include "evohomeclient2/evohomeclient2.hpp"



using namespace std;


std::string mode = "";
std::string argUntil = "";
std::string configfile;

bool verbose;

std::string ERROR = "ERROR: ";
std::string WARN = "WARNING: ";




void exit_error(std::string message)
{
	cerr << message << endl;
	exit(1);
}


void usage(std::string mode)
{
	if (mode == "badparm")
	{
		cout << "Bad parameter" << endl;
		exit(1);
	}
	if (mode == "short")
	{
		cout << "Usage: evo-setmode [-hv] [-c file] <evohome mode>" << endl;
		cout << "Type \"evo-setmode --help\" for more help" << endl;
		exit(0);
	}
	cout << "Usage: evo-setmode [OPTIONS] <evohome mode>" << endl;
	cout << endl;
	cout << "  -v, --verbose           print a lot of information" << endl;
	cout << "  -c, --conf=FILE         use FILE for server settings and credentials" << endl;
	cout << "  -h, --help              display this help and exit" << endl;
	exit(0);
}


void parse_args(int argc, char** argv) {
	int i=1;
	std::string word;
	while (i < argc) {
		word = argv[i];
		if (word.length() > 1 && word[0] == '-' && word[1] != '-') {
			for (size_t j=1;j<word.length();j++) {
				if (word[j] == 'h') {
					usage("short");
				} else if (word[j] == 'v') {
					verbose = true;
				} else if (word[j] == 'c') {
					if (j+1 < word.length())
						usage("badparm");
					i++;
					configfile = argv[i];
				} else {
					usage("badparm");
				}
			}
		} else if (word == "--help") {
			usage("long");
			exit(0);
		} else if (word == "--verbose") {
			verbose = true;
		} else if (word.substr(0,7) == "--conf=") {
			configfile = word.substr(7);
		} else if (mode == "") {
			mode = argv[i];
		} else if (argUntil == "") {
			argUntil = argv[i];
		} else {
			usage("badparm");
		}
		i++;
	}
	if (mode == "")
		usage("short");
}


evohome::device::temperatureControlSystem* select_temperatureControlSystem(EvohomeClient2 &eclient)
{
	int location = 0;
	int gateway = 0;
	int temperatureControlSystem = 0;
	bool is_unique_heating_system = false;
	if ( evoconfig.find("location") != evoconfig.end() ) {
		if (verbose)
			cout << "using location from " << configfile << endl;
		int l = eclient.m_vLocations.size();
		location = atoi(evoconfig["location"].c_str());
		if (location > l)
			exit_error(ERROR+"the Evohome location specified in "+configfile+" cannot be found");
		is_unique_heating_system = ( (eclient.m_vLocations[location].gateways.size() == 1) &&
						(eclient.m_vLocations[location].gateways[0].temperatureControlSystems.size() == 1)
						);
	}
	if ( evoconfig.find("gateway") != evoconfig.end() ) {
		if (verbose)
			cout << "using gateway from " << configfile << endl;
		int l = eclient.m_vLocations[location].gateways.size();
		gateway = atoi(evoconfig["gateway"].c_str());
		if (gateway > l)
			exit_error(ERROR+"the Evohome gateway specified in "+configfile+" cannot be found");
		is_unique_heating_system = (eclient.m_vLocations[location].gateways[gateway].temperatureControlSystems.size() == 1);
	}
	if ( evoconfig.find("controlsystem") != evoconfig.end() ) {
		if (verbose)
			cout << "using controlsystem from " << configfile << endl;
		int l = eclient.m_vLocations[location].gateways[gateway].temperatureControlSystems.size();
		temperatureControlSystem = atoi(evoconfig["controlsystem"].c_str());
		if (temperatureControlSystem > l)
			exit_error(ERROR+"the Evohome temperature controlsystem specified in "+configfile+" cannot be found");
		is_unique_heating_system = true;
	}


	if ( ! is_unique_heating_system)
		return NULL;

	return &eclient.m_vLocations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];
}


void log(const std::string message)
{
	if (verbose)
		cout << message << "\n";
}


int main(int argc, char** argv)
{
	configfile = CONF_FILE;
	parse_args(argc, argv);

	std::string s_until = "";
	if (!argUntil.empty())
	{
		// until set
		if (argUntil.length() < 10)
			exit_error(ERROR+"bad timestamp value on command line");
		struct tm ltime;
		ltime.tm_isdst = -1;
		ltime.tm_year = atoi(argUntil.substr(0, 4).c_str()) - 1900;
		ltime.tm_mon = atoi(argUntil.substr(5, 2).c_str()) - 1;
		ltime.tm_mday = atoi(argUntil.substr(8, 2).c_str());
		ltime.tm_hour = 0;
		ltime.tm_min = 0;
		ltime.tm_sec = 0;
		time_t ntime = mktime(&ltime);
		if ( ntime == -1)
			exit_error(ERROR+"bad timestamp value on command line");
		char c_until[40];
		sprintf(c_until,"%04d-%02d-%02d",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday);
		s_until = string(c_until);
	}



	read_evoconfig();

	log("connect to Evohome server");
	EvohomeClient2 eclient = EvohomeClient2();
	if (eclient.load_auth_from_file(AUTH_FILE_V2))
		log("    reusing saved connection (UK/EMEA)");
	else
	{
		if (eclient.login(evoconfig["usr"],evoconfig["pw"]))
		{
			log("    connected (UK/EMEA)");
			eclient.save_auth_to_file(AUTH_FILE_V2);
		}
		else
			log("    login failed (UK/EMEA)");
	}

	std::string systemId;
	if ( evoconfig.find("systemId") != evoconfig.end() ) {
		if (verbose)
			cout << "using systemId from " << CONF_FILE << endl;
		systemId = evoconfig["systemId"];
	}
	else
	{
		log("retrieve Evohome installation info");
		eclient.full_installation();

		// set Evohome heating system
		evohome::device::temperatureControlSystem *tcs = NULL;

		if (eclient.is_single_heating_system())
			tcs = &eclient.m_vLocations[0].gateways[0].temperatureControlSystems[0];
		else
			select_temperatureControlSystem(eclient);

		if (tcs == NULL)
			exit_error(ERROR+"multiple Evohome systems found - don't know which one to use");

		systemId = tcs->szSystemId;
	}


	if ( ! eclient.set_system_mode(systemId, mode, s_until) )
		exit_error(ERROR+"failed to set system mode to "+mode);
	
	log("updated system status to "+mode);

	eclient.cleanup();

	return 0;
}

