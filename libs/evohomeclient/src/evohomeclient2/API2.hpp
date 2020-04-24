/*
 * Copyright (c) 2016-2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for UK/EMEA Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#pragma once
#include <string>

#define EVOHOME_HOST "https://tccna.honeywell.com"


namespace evohome {

  namespace schedule {
    static const std::string dayOfWeek[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  }; // namespace schedule

  namespace API2 {

    namespace header {
//      static const std::string authkey = "Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=";
      static const std::string authkey = "Authorization: Basic NGEyMzEwODktZDJiNi00MWJkLWE1ZWItMTZhMGE0MjJiOTk5OjFhMTVjZGI4LTQyZGUtNDA3Yi1hZGQwLTA1OWY5MmM1MzBjYg==";
      static const std::string accept = "Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml";
      static const std::string jsondata = "Content-Type: application/json";

    }; // namespace auth

    namespace system {
      static const std::string mode[7] = {"Auto", "HeatingOff", "AutoWithEco", "Away", "DayOff", "", "Custom"};
    }; // namespace system

    namespace zone {
      static const std::string mode[7] = {"FollowSchedule", "PermanentOverride", "TemporaryOverride", "OpenWindow", "LocalOverride", "RemoteOverride", "Unknown"};
      static const std::string type[2] = {"temperatureZone", "domesticHotWater"};
    }; // namespace zone

    namespace dhw {
      static const std::string state[2] = {"Off", "On"};
    }; // namespace dhw

    namespace uri {
      static const std::string base = EVOHOME_HOST"/WebAPI/emea/api/v1/";

      static const std::string userAccount = "userAccount";
      static const std::string installationInfo = "location/installationInfo?includeTemperatureControlSystems=True&userId={id}";
      static const std::string status = "location/{id}/status?includeTemperatureControlSystems=True";
      static const std::string systemMode = "temperatureControlSystem/{id}/mode";
      static const std::string zoneSetpoint = "temperatureZone/{id}/heatSetpoint";
      static const std::string dhwState = "domesticHotWater/{id}/state";
      static const std::string zoneSchedule = "{type}/{id}/schedule";
      static const std::string zoneUpcoming = "{type}/{id}/schedule/upcommingSwitchpoints?count=1";


      static std::string get_uri(const std::string &szApiFunction, const std::string &szId = "", const uint8_t zoneType = 0)
      {
        std::string result = szApiFunction;

        if (szApiFunction == userAccount)
        {
          // all done
        }
        else if (szApiFunction == installationInfo)
          result.replace(71, 4, szId);
        else if (szApiFunction == status)
          result.replace(9, 4, szId);
        else if ((szApiFunction == zoneSchedule) || (szApiFunction == zoneUpcoming))
        {
          result.replace(7, 4, szId);
          result.replace(0, 6, evohome::API2::zone::type[zoneType]);
        }
        else if (szApiFunction == systemMode)
          result.replace(25, 4, szId);
        else if (szApiFunction == zoneSetpoint)
          result.replace(16, 4, szId);
        else if (szApiFunction == dhwState)
          result.replace(17, 4, szId);
        else
          return ""; // invalid input

        return result.insert(0, evohome::API2::uri::base);
      }


    }; // namespace uri

  }; // namespace API2

}; // namespace evohome


