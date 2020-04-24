/*
 * Copyright (c) 2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Jsoncpp bridge for Evohome
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#pragma once
#ifndef _EvohomeJsonBridge
#define _EvohomeJsonBridge

#include <string>
#include "jsoncpp/json.h"


namespace evohome {


static int parse_json_string(std::string &szInput, Json::Value &jOutput)
{
	int res = 0;
	if (szInput[0] == '[')
	{
		// parser cannot handle unnamed arrays
		szInput[0] = ' ';
		int len = static_cast<int>(szInput.size());
		len--;
		szInput[len] = ' ';
		res = 1;
	}

	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szInput.c_str(), szInput.c_str() + szInput.size(), &jOutput, nullptr))
		return -1;
	return res;
}

}; // namespace evohome

#endif
