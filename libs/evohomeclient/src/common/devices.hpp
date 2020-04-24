/*
 * Copyright (c) 2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Type definitions for devices in Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#pragma once
#include <vector>
#include <string>
#include "jsoncpp/json.h"


namespace evohome {
  namespace device {

    typedef struct _sZone // also used for Domestic Hot Water
    {
      uint8_t zoneIdx;
      uint8_t systemIdx;
      uint8_t gatewayIdx;
      uint8_t locationIdx;
      std::string szZoneId;
      Json::Value *jInstallationInfo;
      Json::Value *jStatus;
      Json::Value jSchedule;
    } zone;

    typedef struct _sTemperatureControlSystem
    {
      uint8_t systemIdx;
      uint8_t gatewayIdx;
      uint8_t locationIdx;
      std::string szSystemId;
      Json::Value *jInstallationInfo;
      Json::Value *jStatus;
      std::vector<evohome::device::zone> zones;
      std::vector<evohome::device::zone> dhw;
    } temperatureControlSystem;

    typedef struct _sGateway
    {
      uint8_t gatewayIdx;
      uint8_t locationIdx;
      std::string szGatewayId;
      Json::Value *jInstallationInfo;
      Json::Value *jStatus;
      std::vector<evohome::device::temperatureControlSystem> temperatureControlSystems;
    } gateway;

    typedef struct _sLocation
    {
      uint8_t locationIdx;
      std::string szLocationId;
      Json::Value *jInstallationInfo;
      Json::Value jStatus;
      std::vector<evohome::device::gateway> gateways;
    } location;


    namespace path
    {
      typedef struct _sZone
      {
        uint8_t locationIdx;
        uint8_t gatewayIdx;
        uint8_t systemIdx;
        uint8_t zoneIdx;
        std::string szZoneId;
      } zone;

    }; // namespace path

  }; // namespace device

}; // namespace evohome


