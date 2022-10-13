/*
 *  Local Tuya provider for Oikomaticz
 *
 *  Copyright 2022 - gordonb3 https://github.com/gordonb3/tuyapp
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <https://github.com/gordonb3/tuyapp/blob/master/LICENSE>
 */

#include "stdafx.h"
#include "LocalTuya.hpp"
#include "main/Helper.h"
#include "main/Logger.h"
#include "main/SQLHelper.h"
#include "main/mainworker.h"
#include "hardware/hardwaretypes.h"
#include "main/json_helper.h"
#include "main/WebServer.h"
#include <string>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define CUSTOM_IMAGE_ID 19		// row index inside 'switch_icons.txt'


CLocalTuya::CLocalTuya(const int ID)
{
	m_HwdID = ID;
	m_bSkipReceiveCheck = true;
}


CLocalTuya::~CLocalTuya(void)
{
	m_bIsStarted=false;
}


void CLocalTuya::Init()
{
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT nValue FROM DeviceStatus WHERE (HardwareID=%d) AND (DeviceID='FFFFFFFF') AND (Unit=1) AND (Type=%d) AND (SubType=%d) AND (SwitchType=%d)",
		m_HwdID, pTypeGeneralSwitch, sSwitchGeneralSwitch, int(device::tswitch::type::OnOff));
	if (result.empty())
	{
		GeneralSwitch pSwitch;
		pSwitch.type = pTypeGeneralSwitch;
		pSwitch.subtype = sSwitchGeneralSwitch;
		pSwitch.id = 0xFFFFFFFF;
		pSwitch.unitcode = 1;
		pSwitch.cmnd = 1;
		pSwitch.seqnbr = 0;
		m_mainworker.PushAndWaitRxMessage(this, (const unsigned char *)&pSwitch, "Low Tariff", 255, m_Name.c_str());

		// wait a maximum of 1 second for mainworker to finish adding the device
		int i=10;
		while (i && result.empty())
		{
			result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (DeviceID='FFFFFFFF') AND (Unit=1) AND (Type=%d) AND (SubType=%d)",
				m_HwdID, pTypeGeneralSwitch, sSwitchGeneralSwitch);

			if (result.empty())
			{
				sleep_milliseconds(100);
				i--;
			}
		}

		//Set SwitchType and CustomImage
		int iconID = CUSTOM_IMAGE_ID;
		m_sql.safe_query("UPDATE DeviceStatus SET SwitchType=%d, CustomImage=%d WHERE (HardwareID=%d) AND (DeviceID='FFFFFFFF') AND (Unit=1) AND (Type=%d) AND (SubType=%d)", int(device::tswitch::type::OnOff), iconID, m_HwdID, pTypeGeneralSwitch, sSwitchGeneralSwitch);
	}
	else
	{
		m_tariff = atoi(result[0][0].c_str());
	}
}


bool CLocalTuya::StartHardware()
{
	RequestStart();

	Init();
	m_thread = std::make_shared<std::thread>([this] { Do_Work(); });
	SetThreadNameInt(m_thread->native_handle());
	if (m_thread == nullptr)
		return false;
	m_bIsStarted = true;
	sOnConnected(this);
	return true;
}


bool CLocalTuya::StopHardware()
{
	Log(LOG_STATUS, "Stopping tuya communication threads");

	// we don't want to concatenate receive timeouts on our devices, so first inform communication threads that they should not start any new send/receive
	for (auto &device : m_tuyadevices)
	{
		TuyaData* devicedata = device->m_devicedata;
		devicedata->connectstate = device::tuya::connectstate::STOPPING;
	}

	// stop communication threads
	for (auto &device : m_tuyadevices)
	{
		device->StopMonitor();
		TuyaData* devicedata = device->m_devicedata;
		if (devicedata->connectstate != device::tuya::connectstate::STOPPED)
			Log(LOG_ERROR, "Failed to stop tuya communication thread with %s", devicedata->deviceName);
		delete device;
	}
	m_tuyadevices.clear();

	// stop master thread ([this])
	if (m_thread != nullptr)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	m_bIsStarted = false;
	return true;
}


void CLocalTuya::LoadMeterStartData(TuyaMonitor *tuyadevice, const int DeviceID, const int energyDivider)
{
	if (energyDivider == 0)
		return;
	std::vector<std::vector<std::string>> result;
	result = m_sql.safe_query("SELECT sValue FROM DeviceStatus WHERE (HardwareID=%d) AND (DeviceID='%d') AND (Unit=1)", m_HwdID, DeviceID);
	if (!result.empty())
	{
		for (const auto &sd : result)
		{
			std::string sValue = sd[0];
			std::vector<std::string> splitresults;
			StringSplit(sValue, ";", splitresults);
			unsigned long long powerusage1 = std::strtoull(splitresults[0].c_str(), nullptr, 10);
			unsigned long long powerusage2 = std::strtoull(splitresults[1].c_str(), nullptr, 10);
			unsigned long long powerdeliv1 = std::strtoull(splitresults[2].c_str(), nullptr, 10);
			unsigned long long powerdeliv2 = std::strtoull(splitresults[3].c_str(), nullptr, 10);
			float meterMultiplier = (energyDivider < 0) ? (energyDivider * (-1.0)) : (energyDivider * 1.0);
			tuyadevice->SetMeterStartData((powerusage2 + powerdeliv2)*meterMultiplier, (powerusage1 + powerdeliv1)*meterMultiplier);
		}
	}
}


void CLocalTuya::Do_Work()
{
	// connect to devices
	Log(LOG_STATUS, "Setup tuya communication threads");
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT ID,Name,DeviceID,LocalKey,IPAddress,EnergyDivider FROM TuyaDevices WHERE (HardwareID=%d)", m_HwdID);
	if (!result.empty())
	{
		for (const auto &sd : result)
		{
			uint32_t ID = atoi(sd[0].c_str());
			int energyDivider = atoi(sd[5].c_str());
			TuyaMonitor* tuyadevice = new TuyaMonitor(ID, sd[1], sd[2], sd[3], sd[4], energyDivider);
			tuyadevice->sigSendMeter.connect([this](auto devicedata) {SendMeter(devicedata);});
			tuyadevice->sigSendSwitch.connect([this](auto devicedata) {SendSwitch(devicedata);});
			tuyadevice->sigSendVoltage.connect([this](auto devicedata) {SendVoltage(devicedata);});
			LoadMeterStartData(tuyadevice, ID, energyDivider);
			Log(LOG_NORM, "Setup communication thread with %s", (tuyadevice->m_devicedata)->deviceName);
			if (tuyadevice->StartMonitor())
				Log(LOG_STATUS, "Successfully connected to %s", (tuyadevice->m_devicedata)->deviceName);
			else
				Log(LOG_ERROR, "Failed connect to %s", (tuyadevice->m_devicedata)->deviceName);
			m_tuyadevices.push_back(tuyadevice);
		}
	}


	unsigned long sec_counter = 0;
	while (!IsStopRequested(1000))
	{
		sec_counter++;
		if (sec_counter % 12 == 0) {
			m_LastHeartbeat = mytime(NULL);
		}

		if (sec_counter % 2 == 0) {
			// track disconnected devices
			for (auto &device : m_tuyadevices)
			{
				TuyaData* devicedata = device->m_devicedata;
				if (devicedata->connectstate == device::tuya::connectstate::RESETBYPEER)
				{
					_log.Debug(DEBUG_HARDWARE, "connection with device %s was reset by peer", devicedata->deviceName);
					device->StopMonitor();
					while (devicedata->connectstate != device::tuya::connectstate::STOPPED)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(200));
					}
					devicedata->connectstate = device::tuya::connectstate::OFFLINE;
				}
				if (devicedata->connectstate == device::tuya::connectstate::OFFLINE)
				{
					Log(LOG_NORM, "Retry communication thread with %s", devicedata->deviceName);
					if (device->StartMonitor())
						Log(LOG_STATUS, "Successfully connected to %s", devicedata->deviceName);
				}
			}
		}
	}
}


bool CLocalTuya::WriteToHardware(const char *pdata, const unsigned char length)
{
	if(!pdata)
		return false;

	// switch command
	const _tGeneralSwitch *pSwitch = reinterpret_cast<const _tGeneralSwitch*>(pdata);
	if (pSwitch->type != pTypeGeneralSwitch)
		return false; // only allowed to control regular switches

	int cmnd = pSwitch->cmnd;

	if (pSwitch->id == (uint32_t)(-1))
	{
		// signal energy meters to switch tariff
		if (cmnd == gswitch_sOn) // low tariff
		{
			Log(LOG_STATUS, "Enabling low tariff");
			m_tariff = 1;
		}
		else
		{
			Log(LOG_STATUS, "Enabling high tariff");
			m_tariff = 0; // high/single tariff
		}
		return true;
	}

	// send switch command to device communication thread
	for (auto &device : m_tuyadevices)
	{
		TuyaData* devicedata = device->m_devicedata;
		if (devicedata->deviceID == pSwitch->id)
		{
			if (devicedata->connectstate != device::tuya::connectstate::CONNECTED)
			{
				Log(LOG_ERROR, "Device %s is offline", devicedata->deviceName);
				return false;
			}
			if (device->SendSwitchCommand(cmnd))
				return true;
			// looks like we were silently dropped - force a reconnect
			Log(LOG_ERROR, "Failed to send switch command to %s, forcing a reconnect", devicedata->deviceName);
			devicedata->connectstate = device::tuya::connectstate::RESETBYPEER;
			return false;
		}
	}

	return false;
}


void CLocalTuya::SendMeter(TuyaData *devicedata)
{
	if ((devicedata->usageHigh > 0) || (devicedata->usageLow > 0))
	{
		P1Power	tuya_energy;
		memset(&tuya_energy, 0, sizeof(P1Power));
		tuya_energy.len = sizeof(P1Power) - 1;
		tuya_energy.type = pTypeP1Power;
		tuya_energy.subtype = sTypeP1Power;
		tuya_energy.ID = devicedata->deviceID;

		if (devicedata->energyDivider < 0)
		{
			int energyDivider = devicedata->energyDivider*(-1);
			tuya_energy.delivcurrent = devicedata->power/energyDivider;
			tuya_energy.powerdeliv1 = devicedata->usageLow/energyDivider;
			tuya_energy.powerdeliv2 = devicedata->usageHigh/energyDivider;
		}
		else if (devicedata->energyDivider > 1)
		{
			int energyDivider = devicedata->energyDivider;
			tuya_energy.usagecurrent = devicedata->power/energyDivider;
			tuya_energy.powerusage1 = devicedata->usageLow/energyDivider;
			tuya_energy.powerusage2 = devicedata->usageHigh/energyDivider;
		}
		else
		{
			tuya_energy.usagecurrent = devicedata->power;
			tuya_energy.powerusage1 = devicedata->usageLow;
			tuya_energy.powerusage2 = devicedata->usageHigh;
		}
		sDecodeRXMessage(this, (const unsigned char *)&tuya_energy, devicedata->deviceName, 255, nullptr);
	}
	if (devicedata->isLowTariff != m_tariff)
		devicedata->isLowTariff = m_tariff;
}


void CLocalTuya::SendSwitch(TuyaData *devicedata)
{
	uint8_t unitcode = 2; // P1 meter claims unit code 1

	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT nValue FROM DeviceStatus WHERE (HardwareID=%d) AND (DeviceID='%08d') AND (Unit=%d) AND (Type=%d) AND (Subtype=%d)", m_HwdID, devicedata->deviceID, unitcode, pTypeGeneralSwitch, sSwitchTypeAC);
	if (!result.empty())
	{
		//check if we have a change, if not do not update it
		int currentState = atoi(result[0][0].c_str());
		if (currentState == (int)devicedata->switchstate)
			return;
	}

	GeneralSwitch tuya_switch;
	memset(&tuya_switch, 0, sizeof(GeneralSwitch));
	tuya_switch.len = sizeof(GeneralSwitch) - 1;
	tuya_switch.type = pTypeGeneralSwitch;
	tuya_switch.subtype = sSwitchTypeAC;
	tuya_switch.switchtype = device::tswitch::type::OnOff;
	tuya_switch.id = devicedata->deviceID;
	tuya_switch.unitcode = unitcode;
	tuya_switch.cmnd = (uint8_t)devicedata->switchstate;
	sDecodeRXMessage(this, (const unsigned char *)&tuya_switch, devicedata->deviceName, 255, nullptr);
}

void CLocalTuya::SendVoltage(TuyaData *devicedata)
{
	GeneralDevice tuya_voltmeter;
	tuya_voltmeter.subtype = sTypeVoltage;
	tuya_voltmeter.id = devicedata->deviceID;
	tuya_voltmeter.intval1 = devicedata->deviceID | 0x30000;
	tuya_voltmeter.floatval1 = (float)(devicedata->voltage) / 10;
	sDecodeRXMessage(this, (const unsigned char *)&tuya_voltmeter, devicedata->deviceName, 255, nullptr);
}

//Webserver helpers
namespace http {
	namespace server {

		void CWebServer::Cmd_TuyaGetDevices(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}
			std::string hwid = request::findValue(&req, "idx");
			if (hwid.empty())
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pHardware = m_mainworker.GetHardware(iHardwareID);
			if (pHardware == nullptr)
				return;
			if (pHardware->HwdType != hardware::type::LocalTuya)
				return;

			root["status"] = "OK";
			root["title"] = "TuyaGetDevices";

			std::vector<std::vector<std::string> > result;
			result = m_sql.safe_query("SELECT ID,Name,IPAddress,DeviceID,LocalKey,EnergyDivider FROM TuyaDevices WHERE (HardwareID=%d)", iHardwareID);
			if (!result.empty())
			{
				int ii = 0;
				for (const auto &sd : result)
				{
					root["result"][ii]["idx"] = sd[0];
					root["result"][ii]["Name"] = sd[1];
					root["result"][ii]["IP"] = sd[2];
					root["result"][ii]["Tuya_ID"] = sd[3];
					root["result"][ii]["Local_Key"] = sd[4];
					root["result"][ii]["EnergyDivider"] = sd[5];
					ii++;
				}
			}
		}

		void CWebServer::Cmd_AddTuyaDevice(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}
			std::string hwid = request::findValue(&req, "idx");
			if (hwid.empty())
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pHardware = m_mainworker.GetHardware(iHardwareID);
			if (pHardware == nullptr)
				return;
			if (pHardware->HwdType != hardware::type::LocalTuya)
				return;

			std::string devicename = request::findValue(&req, "name");
			std::string tuyaID = request::findValue(&req, "tuyaid");
			std::string localkey = request::findValue(&req, "localkey");
			std::string IP_Address = request::findValue(&req, "ipaddr");
			std::string sEnergyDivider = request::findValue(&req, "energydivider");
			int iEnergyDivider = atoi(sEnergyDivider.c_str());


			//Make a unique number for ID
			int nid = 1;
			std::vector<std::vector<std::string> > result;
			result = m_sql.safe_query("SELECT MAX(ID) FROM TuyaDevices");
			if (!result.empty())
				nid = atoi(result[0][0].c_str()) + 1;

			m_sql.safe_query("INSERT INTO TuyaDevices VALUES (%d,%d,'%s','%s','%s','%s',%d)", nid, iHardwareID, tuyaID.c_str(), localkey.c_str(), IP_Address.c_str(), devicename.c_str(), iEnergyDivider);
			m_mainworker.RestartHardware(hwid);
			root["status"] = "OK";
		}


		void CWebServer::Cmd_UpdateTuyaDevice(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}
			std::string hwid = request::findValue(&req, "idx");
			if (hwid.empty())
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pHardware = m_mainworker.GetHardware(iHardwareID);
			if (pHardware == nullptr)
				return;
			if (pHardware->HwdType != hardware::type::LocalTuya)
				return;

			std::string devid = request::findValue(&req, "devid");
			int nid= atoi(devid.c_str());
			std::string devicename = request::findValue(&req, "name");
			std::string tuyaID = request::findValue(&req, "tuyaid");
			std::string localkey = request::findValue(&req, "localkey");
			std::string IP_Address = request::findValue(&req, "ipaddr");
			std::string sEnergyDivider = request::findValue(&req, "energydivider");
			int iEnergyDivider = atoi(sEnergyDivider.c_str());
_log.Log(LOG_STATUS,"UPDATE TuyaDevices SET DeviceID='%s', LocalKey='%s', IPAddress='%s', Name='%s', energyDivider=%d WHERE (ID=%d) AND (HardwareID=%d)", tuyaID.c_str(), localkey.c_str(), IP_Address.c_str(), devicename.c_str(), iEnergyDivider,nid, iHardwareID);

			m_sql.safe_query("UPDATE TuyaDevices SET DeviceID='%s', LocalKey='%s', IPAddress='%s', Name='%s', energyDivider=%d WHERE (ID=%d) AND (HardwareID=%d)", tuyaID.c_str(), localkey.c_str(), IP_Address.c_str(), devicename.c_str(), iEnergyDivider, nid, iHardwareID);
			m_mainworker.RestartHardware(hwid);
			root["status"] = "OK";
		}

		void CWebServer::Cmd_DeleteTuyaDevice(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}
			std::string hwid = request::findValue(&req, "idx");
			if (hwid.empty())
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pHardware = m_mainworker.GetHardware(iHardwareID);
			if (pHardware == nullptr)
				return;
			if (pHardware->HwdType != hardware::type::LocalTuya)
				return;

			std::string devid = request::findValue(&req, "devid");
			int nid= atoi(devid.c_str());

			m_sql.safe_query("DELETE FROM TuyaDevices WHERE (ID=%d) AND (HardwareID=%d)", nid, iHardwareID);
			m_mainworker.RestartHardware(hwid);
			root["status"] = "OK";
		}
	}
}

