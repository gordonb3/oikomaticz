/*
 * Copyright (c) 2018 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Simple one-way app for sending mode changes to Evohome web portal
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <time.h>
#include <stdlib.h>
#include "demo-defaults.hpp"
#include "evohomeclient2/evohomeclient2.hpp"


#ifndef MYNAME
#define MYNAME "evo-cmd"
#endif


using namespace std;

time_t now;
bool dobackup, verbose, dolog;
std::string command, backupfile, configfile, szERROR, szWARN, logfile, szLOG;
map<int,std::string> parameters;
ofstream flog;

void init_globals()
{
	configfile = CONF_FILE;

	now = time(0);
	dobackup = true;
	verbose = false;
	command = "";

	szERROR = "ERROR: ";
	szWARN = "WARNING: ";

	dolog = false;
	logfile = "";
	szLOG = "";
}


void usage(std::string mode)
{
	if (mode == "badparm")
	{
		cerr << "Bad parameter\n";
		exit(1);
	}
	if (mode == "missingfile")
	{
		cerr << "You need to supply a backupfile\n";
		exit(1);
	}
	if (mode == "short")
	{
		cout << "Usage: " << MYNAME << " [-hv] [-c file] [-l file] [-b|-r file]\n";
		cout << "Type \"" << MYNAME << " --help\" for more help\n";
		exit(0);
	}
	cout << "Usage: " << MYNAME << " [OPTIONS]\n";
	cout << endl;
	cout << "  -h, --help                display this help and exit\n";
	cout << "  -b, --backup  <FILE>      create a schedules backup\n";
	cout << "  -r, --restore <FILE>      restore a schedules backup\n";
	cout << "  -v, --verbose             print a lot of information\n";
	cout << "  -c, --conf=FILE           use FILE for server settings and credentials\n";
	cout << "  -l, --log=FILE            use FILE for logging (implies -v)\n";
	cout << "      --set-mode <PARMS>    set system mode\n";
	cout << "      --set-temp <PARMS>    set or cancel temperature override\n";
	cout << "      --set-dhw  <PARMS>    set or cancel domestic hot water override\n";
	cout << endl;
	cout << "Parameters for set arguments:\n";
	cout << " --set-mode <system mode> [time until] [+duration]\n";
	cout << " --set-temp <zone id> <setpoint mode> <target temperature> [time until] [+duration]\n";
	cout << " --set-dhw  <dhw id> <dhw status> [time until] [+duration]\n";
	cout << endl;
	exit(0);
}


void print_out(std::string message)
{
	if (dolog)
		flog << message << endl;
	cout << message << endl;
}


void print_err(std::string message)
{
	if (dolog)
		flog << message << endl;
	cerr << message << endl;
}


void startlog(std::string fname)
{
	logfile = fname;
	flog.open(logfile.c_str(), ios::out | ios::app);
	if (flog.is_open())
	{
		dolog = true;
		struct tm ltime;
		localtime_r(&now, &ltime);
		char c_until[40];
		sprintf_s(c_until, 40, "%04d-%02d-%02d %02d:%02d:%02d", ltime.tm_year+1900, ltime.tm_mon+1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
		flog << MYNAME << " start: " << c_until << endl;
		return;
	}
	cerr << "WARNING: cannot open logfile '" << logfile << "'\n";
	dolog = false;
	verbose = true;
}


void log(std::string message)
{
	if (dolog)
	{
		flog << message << endl;
		return;
	}
	if (verbose)
		cout << message << endl;
}


void stoplog()
{
	if (!dolog)
		return;
	flog << "--" << endl;
	flog.close();
}


void exit_error(std::string message)
{
	print_err(message);
	stoplog();
	exit(1);
}


void authorize_to_server(EvohomeClient2 &eclient)
{
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
}


evohome::device::temperatureControlSystem* select_temperatureControlSystem(EvohomeClient2 &eclient)
{
	int locationIdx = 0;
	int gatewayIdx = 0;
	int systemIdx = 0;
	bool is_unique_heating_system = false;
	if ( evoconfig.find("location") != evoconfig.end() ) {
		log(szLOG+"using location from "+configfile);
		locationIdx = atoi(evoconfig["location"].c_str());
		if (locationIdx > (int)eclient.m_vLocations.size())
			exit_error(szERROR+"the Evohome location specified in "+configfile+" cannot be found");
		is_unique_heating_system = ( (eclient.m_vLocations[locationIdx].gateways.size() == 1) &&
						(eclient.m_vLocations[locationIdx].gateways[0].temperatureControlSystems.size() == 1)
						);
	}
	if ( evoconfig.find("gateway") != evoconfig.end() ) {
		log(szLOG+"using gateway from "+configfile);
		gatewayIdx = atoi(evoconfig["gateway"].c_str());
		if (gatewayIdx > (int)eclient.m_vLocations[locationIdx].gateways.size())
			exit_error(szERROR+"the Evohome gateway specified in "+configfile+" cannot be found");
		is_unique_heating_system = (eclient.m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems.size() == 1);
	}
	if ( evoconfig.find("controlsystem") != evoconfig.end() ) {
		log(szLOG+"using controlsystem from "+configfile);
		systemIdx = atoi(evoconfig["controlsystem"].c_str());
		if (systemIdx > (int)eclient.m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems.size())
			exit_error(szERROR+"the Evohome temperature controlsystem specified in "+configfile+" cannot be found");
		is_unique_heating_system = true;
	}

	if ( ! is_unique_heating_system)
		return NULL;

	return &eclient.m_vLocations[locationIdx].gateways[gatewayIdx].temperatureControlSystems[systemIdx];
}


void parse_args(int argc, char** argv) {
	int i=1;
	int p=1;
	std::string word;
	while (i < argc) {
		word = argv[i];
		if (word.length() > 1 && word[0] == '-' && word[1] != '-') {
			for (size_t j=1;j<word.length();j++) {
				if (word[j] == 'h')
					usage("short");
				else if (word[j] == 'b')
				{
					if (j+1 < word.length())
						usage("badparm");
					command = "backup";
					dobackup = true;
					i++;
					if (i >= argc)
						usage("missingfile");
					backupfile = argv[i];
				}
				else if (word[j] == 'r')
				{
					if (j+1 < word.length())
						usage("badparm");
					command = "backup";
					dobackup = false;
					i++;
					if (i >= argc)
						usage("missingfile");
					backupfile = argv[i];
				}
				else if (word[j] == 'v')
					verbose = true;
				else if (word[j] == 'c')
				{
					if (j+1 < word.length())
						usage("badparm");
					i++;
					if (i >= argc)
						usage("badparm");
					configfile = argv[i];
				}
				else if (word[j] == 'l')
				{
					if (j+1 < word.length())
						usage("badparm");
					i++;
					if (i >= argc)
						usage("badparm");
					startlog(argv[i]);
				}
				else
					usage("badparm");
			}
		}
		else if (word == "--help")
			usage("long");
		else if (word == "--backup")
		{
			command = "backup";
			dobackup = true;
			i++;
			if (i >= argc)
				usage("missingfile");
			backupfile = argv[i];
		}
		else if (word == "--restore")
		{
			command = "backup";
			dobackup = false;
			i++;
			if (i >= argc)
				usage("missingfile");
			backupfile = argv[i];
		}
		else if (word == "--verbose")
			verbose = true;
		else if (word.substr(0,7) == "--conf=")
			configfile = word.substr(7);
		else if (word.substr(0,6) == "--log=")
			startlog(word.substr(6));
		else if (word.substr(0,6) == "--set-")
		{
			command = word.substr(2);
			i++;
			while ( (i < argc) && (argv[i][0] != '-') )
			{
				parameters[p] = argv[i];
				i++;
				p++;
			}
			continue;
		}
		else
			usage("badparm");
		i++;
	}
}


void cmd_backup_and_restore_schedules()
{
	// connect to Evohome server
	log("connect to Evohome server");
	EvohomeClient2 eclient = EvohomeClient2();
	authorize_to_server(eclient);

	// retrieve Evohome installation
	log("retrieve Evohome installation info");
	eclient.full_installation();

	if (dobackup)	// backup
	{
		print_out("Start backup of Evohome schedules");
		if ( ! eclient.schedules_backup(backupfile) )
			exit_error(szERROR+"failed to open backup file '"+backupfile+"'");
		print_out("Done!");
	}
	else		// restore
	{
		print_out("Start restore of Evohome schedules");
		if ( ! eclient.schedules_restore(backupfile) )
			exit_error(szERROR+"failed to open backup file '"+backupfile+"'");
		print_out("Done!");
	}

	eclient.cleanup();
}


std::string format_time(std::string utc_time)
{
	struct tm ltime;
	if (utc_time[0] == '+')
	{
		int minutes = atoi(utc_time.substr(1).c_str());
		gmtime_r(&now, &ltime);
		ltime.tm_min += minutes;
		
	}
	else if (utc_time.length() < 19)
		exit_error(szERROR+"bad timestamp value on command line");
	else
	{
		ltime.tm_year = atoi(utc_time.substr(0, 4).c_str()) - 1900;
		ltime.tm_mon = atoi(utc_time.substr(5, 2).c_str()) - 1;
		ltime.tm_mday = atoi(utc_time.substr(8, 2).c_str());
		ltime.tm_hour = atoi(utc_time.substr(11, 2).c_str());
		ltime.tm_min = atoi(utc_time.substr(14, 2).c_str());
		ltime.tm_sec = atoi(utc_time.substr(17, 2).c_str());
	}
	ltime.tm_isdst = -1;
	time_t ntime = mktime(&ltime);
	if ( ntime == -1)
		exit_error(szERROR+"bad timestamp value on command line");
	char c_until[40];
	sprintf_s(c_until, 40, "%04d-%02d-%02dT%02d:%02d:%02dZ", ltime.tm_year+1900, ltime.tm_mon+1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	return string(c_until);
}


void cancel_temperature_override()
{
	log("connect to Evohome server");
	EvohomeClient2 eclient = EvohomeClient2();
	authorize_to_server(eclient);

	if ( ! eclient.cancel_temperature_override(parameters[1]) )
		exit_error(szERROR+"failed to cancel override for zone "+parameters[1]);

	eclient.cleanup();
	log("Done!");
}


void cmd_set_temperature()
{
	if ( (parameters.size() > 1) && (parameters[2] == "0") )
	{
		cancel_temperature_override();
		exit(0);
	}
	if ( (parameters.size() < 3) || (parameters.size() > 4) )
		usage("badparm");

	std::string s_until = "";
	if (parameters.size() == 4)
		s_until = format_time(parameters[4]);


	log("connect to Evohome server");
	EvohomeClient2 eclient = EvohomeClient2();
	authorize_to_server(eclient);

	log("set target temperature");

	eclient.set_temperature(parameters[1], parameters[3], s_until);

	eclient.cleanup();
	log("Done!");
}


void cmd_set_system_mode()
{
	if ( (parameters.size() == 0) || (parameters.size() > 2) )
		usage("badparm");
	std::string szSystemId;
	std::string until = "";
	std::string mode = parameters[1];
	if (parameters.size() == 2)
		until = format_time(parameters[2]);
	log("connect to Evohome server");
	EvohomeClient2 eclient = EvohomeClient2();
	authorize_to_server(eclient);

	if ( evoconfig.find("systemId") != evoconfig.end() ) {
		log(szLOG+"using systemId from "+configfile);
		szSystemId = evoconfig["systemId"];
	}
	else
	{
		log("retrieve Evohome installation info");
		eclient.full_installation();

		// set Evohome heating system
		evohome::device::temperatureControlSystem* tcs = NULL;
		if (eclient.is_single_heating_system())
			tcs = &eclient.m_vLocations[0].gateways[0].temperatureControlSystems[0];
		else
			select_temperatureControlSystem(eclient);
		if (tcs == NULL)
			exit_error(szERROR+"multiple Evohome systems found - don't know which one to use");
		szSystemId = tcs->szSystemId;
	}
	if ( ! eclient.set_system_mode(szSystemId, mode, until) )
		exit_error(szERROR+"failed to set system mode to "+mode);
	log("updated system status");
	eclient.cleanup();
}


void cmd_set_dhw_state()
{
	if ( (parameters.size() < 2) || (parameters.size() > 3) )
		usage("badparm");
	if ( (parameters[2] != "on") && (parameters[2] != "off") && (parameters[2] != "auto") )
		usage("badparm");

	std::string s_until = "";
	if (parameters.size() == 3)
		s_until = format_time(parameters[3]);

	log("connect to Evohome server");
	EvohomeClient2 eclient = EvohomeClient2();
	authorize_to_server(eclient);

	log("set domestic hot water state");
	eclient.set_dhw_mode(parameters[1], parameters[2], s_until);
	eclient.cleanup();
	log("Done!");
}


std::string getpath(std::string filename)
{
#ifdef _WIN32
	stringstream ss;
	unsigned int i;
	for (i = 0; i < filename.length(); i++)
	{
		if (filename[i] == '\\')
			ss << '/';
		else
			ss << filename[i];
	}
	filename = ss.str();
#endif
	std::size_t pos = filename.rfind('/');
	if (pos == std::string::npos)
		return "";
	return filename.substr(0, pos+1);
}


int main(int argc, char** argv)
{
	init_globals();
	parse_args(argc, argv);

	if (command.empty())
		usage("long");

	if ( ( ! read_evoconfig(configfile) ) && (getpath(configfile) == "") )
	{
		// try to find evoconfig in the application path
		stringstream ss;
		ss << getpath(argv[0]) << configfile;
		if ( ! read_evoconfig(ss.str()) )
			exit_error(szERROR+"can't read config file");
	}

	if ( ( ! dolog ) && (evoconfig.find("logfile") != evoconfig.end()) )
		startlog(evoconfig["logfile"]);

	else if (command == "backup")
	{
		cmd_backup_and_restore_schedules();
	}

	else if (command == "set-mode")
	{
		cmd_set_system_mode();
	}

	else if (command == "set-temp")
	{
		cmd_set_temperature();
	}
	
	else if (command == "set-dhw")
	{
		cmd_set_dhw_state();
	}


	stoplog();
	return 0;
}

