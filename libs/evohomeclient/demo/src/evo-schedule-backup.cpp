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
#include <time.h>
#include "demo-defaults.hpp"
#include "evohomeclient2/evohomeclient2.hpp"


#ifndef BACKUP_FILE
#define BACKUP_FILE "schedules.backup"
#endif


using namespace std;

std::string backupfile;
std::string configfile;

bool dobackup = true;
bool verbose;


std::string szERROR = "ERROR: ";
std::string szWARN = "WARNING: ";


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
		cout << "Usage: evo-schedule-backup [-hrv] [-c file] [-f file]" << endl;
		cout << "Type \"evo-schedule-backup --help\" for more help" << endl;
		exit(0);
	}
	cout << "Usage: evo-schedule-backup [OPTIONS]" << endl;
	cout << endl;
	cout << "  -r, --restore           restore a previous backup" << endl;
	cout << "  -v, --verbose           print a lot of information" << endl;
	cout << "  -c, --conf=FILE         use FILE for server settings and credentials" << endl;
	cout << "  -f, --file=FILE         use FILE for backup and restore" << endl;
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
					exit(0);
				} else if (word[j] == 'r') {
					dobackup = false;
				} else if (word[j] == 'v') {
					verbose = true;
				} else if (word[j] == 'c') {
					if (j+1 < word.length())
						usage("badparm");
					i++;
					configfile = argv[i];
				} else if (word[j] == 'f') {
					if (j+1 < word.length())
						usage("badparm");
					i++;
					backupfile = argv[i];
				} else {
					usage("badparm");
					exit(1);
				}
			}
		} else if (word == "--help") {
			usage("long");
			exit(0);
		} else if (word == "--restore") {
			dobackup = false;
		} else if (word == "--verbose") {
			verbose = true;
		} else if (word.substr(0,7) == "--conf=") {
			configfile = word.substr(7);
		} else if (word.substr(0,7) == "--file=") {
			backupfile = word.substr(7);
		} else {
			usage("badparm");
			exit(1);
		}
		i++;
	}
}

void log(const std::string message)
{
	if (verbose)
		cout << message << "\n";
}


int main(int argc, char** argv)
{

	backupfile = BACKUP_FILE;
	configfile = CONF_FILE;
	parse_args(argc, argv);

// override defaults with settings from config file
	read_evoconfig();

// connect to Evohome server
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

// retrieve Evohome installation
	log("retrieve Evohome installation info");
	eclient.full_installation();

	if (dobackup)	// backup
	{
		cout << "Start backup of Evohome schedules\n";
		if ( ! eclient.schedules_backup(backupfile) )
			exit_error(szERROR+"failed to open backup file '"+backupfile+"'");
		cout << "Done!\n";
	}
	else		// restore
	{
		cout << "Start restore of Evohome schedules\n";
		if ( ! eclient.schedules_restore(backupfile) )
			exit_error(szERROR+"failed to open backup file '"+backupfile+"'");
		cout << "Done!\n";
	}

	eclient.cleanup();

	return 0;
}

