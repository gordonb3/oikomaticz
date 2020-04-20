/*
 * Copyright (c) 2018 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Simple one-way app for sending mode changes to Evohome web portal
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */


#ifndef MYPATH
#define MYPATH "/"
#endif

#ifndef CONF_FILE
#define CONF_FILE "evoconfig"
#endif

#ifndef SCHEDULE_CACHE
#define SCHEDULE_CACHE "schedules.json"
#endif

#ifndef AUTH_FILE_V1
#define AUTH_FILE_V1 "/tmp/evo1auth.json"
#endif

#ifndef AUTH_FILE_V2
#define AUTH_FILE_V2 "/tmp/evo2auth.json"
#endif


#ifdef _WIN32
#define localtime_r(timep, result) localtime_s(result, timep)
#define gmtime_r(timep, result) gmtime_s(result, timep)
#endif

#ifndef _WIN32
#define sprintf_s(buffer, buffer_size, stringbuffer, ...) (sprintf(buffer, stringbuffer, __VA_ARGS__))
#endif


#include <fstream>
#include <sstream>
#include <map>


std::map<std::string,std::string> evoconfig;


bool read_evoconfig(std::string configfile = CONF_FILE)
{
	std::ifstream myfile (configfile.c_str());
	if ( myfile.is_open() )
	{
		std::stringstream key,val;
		bool isKey = true;
		bool quoted = false;
		std::string line;
		unsigned int i;
		while ( getline(myfile,line) )
		{
			if ( (line[0] == '#') || (line[0] == ';') )
				continue;
			for (i = 0; i < line.length(); i++)
			{
				if ( (line[i] == '\'') || (line[i] == '"') )
				{
					quoted = ( ! quoted );
					continue;
				}
				if (line[i] == 0x0d)
					continue;
				if ( (line[i] == ' ') && ( ! quoted ) )
					continue;
				if (line[i] == '=')
				{
					isKey = false;
					continue;
				}
				if (isKey)
					key << line[i];
				else
					val << line[i];
			}
			if ( ! isKey )
			{
				std::string skey = key.str();
				evoconfig[skey] = val.str();
				isKey = true;
				key.str("");
				val.str("");
			}
		}
		myfile.close();
		return true;
	}
	return false;
}


