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

  namespace API {

    namespace auth {
	static const std::string applicationId = "91db1612-73fd-4500-91b2-e63b069b185c";
    }; // namespace auth

    namespace header {
      static const std::string accept = "Accept: application/json, application/xml, text/json, text/x-json, text/xml";
      static const std::string jsondata = "Content-Type: application/json";

    }; // namespace header

    namespace device {
      static const std::string mode[7] = {"Scheduled", "Hold", "Temporary", "", "", "", ""};
      static const std::string type[2] = {"EMEA_ZONE", "DOMESTIC_HOT_WATER"};
      static const std::string state[2] = {"DHWOff", "DHWOn"};
    }; // namespace device

    namespace uri {
      static const std::string base = EVOHOME_HOST"/WebAPI/api/";

      static const std::string login = "Session";
      static const std::string installationInfo = "locations/?allData=True&userId={id}";
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



