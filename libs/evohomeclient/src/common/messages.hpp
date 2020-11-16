/*
 * Copyright (c) 2016-2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for 'Old' US Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#pragma once
#include <string>

#define EVOHOME_HOST "https://tccna.honeywell.com"


namespace evohome {

  namespace messages {

    static const std::string cmdRejected = "Command was rejected";
    static const std::string invalidTimestamp = "Invalid timestamp";
    static const std::string invalidAuthfile = "Failed to parse auth file content as JSON";
    static const std::string invalidResponse = "Failed to parse server response as JSON";
    static const std::string unhandledResponse = "Server returned an unhandled response";

  }; // namespace messages

}; // namespace evohome



