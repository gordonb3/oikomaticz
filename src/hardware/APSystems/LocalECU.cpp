/*
 *  APS Local ECU provider for Oikomaticz
 *
 *  Copyright 2024 - gordonb3 https://github.com/gordonb3/apsystems-ecupp
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <https://github.com/gordonb3/apsystems-ecupp/blob/master/LICENSE>
 */

#include "stdafx.h"
#include "LocalECU.hpp"
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

#define CUSTOM_IMAGE_ID 19		// row index inside 'switch_icons.txt' for the tariff switch
#define REPORT_INTERVAL 300
#define RETRY_INTERVAL 15


CAPSLocalECU::CAPSLocalECU(const int ID, const std::string &IPAddress) : m_IPAddress(IPAddress)
{
	m_HwdID = ID;
	m_bSkipReceiveCheck = true;
}


CAPSLocalECU::~CAPSLocalECU(void)
{
	m_bIsStarted=false;
}


void CAPSLocalECU::Init()
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
		int i = 10;
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

	result = m_sql.safe_query("SELECT ID,sValue FROM DeviceStatus WHERE (HardwareID=%d) AND (Type=%d)", m_HwdID, pTypeP1Power);
	if (!result.empty())
	{
		for (const auto &sd : result)
		{
			std::string sValue = sd[1];
			std::vector<std::string> splitresults;
			StringSplit(sValue, ";", splitresults);
			m_usageLow = std::strtoul(splitresults[0].c_str(), nullptr, 10);
			m_usageHigh = std::strtoul(splitresults[1].c_str(), nullptr, 10);
		}
	}
	else
	{
		m_usageLow = 0;
		m_usageHigh = 0;
	}
	m_todayEnergyOffset = -1;
	m_lastLifeEnergy = m_usageLow + m_usageHigh;
}


bool CAPSLocalECU::StartHardware()
{
	Log(LOG_STATUS, "Starting Local ECU thread");

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


bool CAPSLocalECU::StopHardware()
{
	Log(LOG_STATUS, "Stopping Local ECU thread");

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


void CAPSLocalECU::Do_Work()
{
	m_ECUClient = new ecuAPI();
	m_ECUClient->SetTargetAddress(m_IPAddress);
	int seconds_remaining = 0;

	unsigned long sec_counter = 0;
	while (!IsStopRequested(1000))
	{
		sec_counter++;
		if (sec_counter % 12 == 0) {
			m_LastHeartbeat = mytime(NULL);
		}

		seconds_remaining--;
		if (seconds_remaining <= 0)
		{
			seconds_remaining = RETRY_INTERVAL;
			if (GetECUData())
				seconds_remaining = (int)(m_ECULastReport + REPORT_INTERVAL - mytime(NULL));
		}
	}
}


// Tariff switch
bool CAPSLocalECU::WriteToHardware(const char *pdata, const unsigned char length)
{
	/* Some of the ECU units expose a web based interface allowing the user to send a limited set
	 * of commands including a rest of the main communication port.
	 *
	 * For now the only application of this WriteToHardware() function is to toggle a parameter
	 * inside this module to split the ECU's single counter into a double tariff counter. This
	 * switch should be kept in sync with the tariff indicator from your smart meter.
	 */
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
			Log(LOG_STATUS, "ECU: Enabling low tariff");
			m_tariff = 1;
		}
		else
		{
			Log(LOG_STATUS, "ECU: Enabling high tariff");
			m_tariff = 0; // high tariff
		}
		return true;
	}

	return false;
}


bool CAPSLocalECU::GetECUData()
{
	/* The ECU is incapable of simultaneous communication. When it is communicating with the inverters
	 * (using Zigbe) it will thus reject our communication attempt. The same happens when the ECU is
	 * "phoning home" to update the numbers accessable through the web based app.
	 *
	 * TODO whenever the ECU starts returning errno 111 it requires either a restart or activation of
	 * its AP before allowing us access again. For now this will require user intervention.
	 */
	int statuscode;
	statuscode = m_ECUClient->QueryECU();
	if (statuscode == 0)
		statuscode = m_ECUClient->QueryInverters();
	if ((statuscode == 0) && (m_ECULastReport < m_ECUClient->m_apsecu.timestamp))
	{
		_log.Debug(DEBUG_HARDWARE, "Proces ECU data");
		m_ECULastReport = m_ECUClient->m_apsecu.timestamp;
		SendMeters();
		return true;
	}
	if (statuscode == -1)
		_log.Debug(DEBUG_HARDWARE, "attempt to connect to ECU returned error %d", errno);
	else if (statuscode == 1)
		_log.Debug(DEBUG_HARDWARE, "ECU returned invalid data");
	return false;
}


std::string CAPSLocalECU::GetP1IDx()
{
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d')", m_HwdID, pTypeP1Power, sTypeP1Power);
	if (result.empty())
	{
		_log.Debug(DEBUG_HARDWARE, "ECU: Create solar power meter");
		P1Power	ecu_energy;
		memset(&ecu_energy, 0, sizeof(P1Power));
		ecu_energy.len = sizeof(P1Power) - 1;
		ecu_energy.type = pTypeP1Power;
		ecu_energy.subtype = sTypeP1Power;
		ecu_energy.ID = 1;
		ecu_energy.usagecurrent = m_ECUClient->m_apsecu.current_power;
		ecu_energy.powerusage1 = m_usageLow;
		ecu_energy.powerusage2 = (m_ECUClient->m_apsecu.lifetime_energy * 1000) - m_usageLow;
		sDecodeRXMessage(this, (const unsigned char *)&ecu_energy, "Solar Power", 255, nullptr);
		while (result.empty()) // wait for device to be created
		{
			sleep_milliseconds(100);
			result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d')", m_HwdID, pTypeP1Power, sTypeP1Power);
		}
	}
	return result[0][0];
}


std::string CAPSLocalECU::GetVoltmeterIDx(const std::string &szShortID)
{
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (DeviceID='%s')", m_HwdID, pTypeGeneral, sTypeVoltage, szShortID.c_str());
	if (result.empty())
	{
		_log.Debug(DEBUG_HARDWARE, "ECU: Create voltage meter for inverter %s", szShortID.c_str());
		char *end = NULL;
		int inverterID = (int)strtoul(szShortID.c_str(), &end, 16);
		_tGeneralDevice gDevice;
		gDevice.subtype = sTypeVoltage;
		gDevice.id = 0;
		gDevice.intval1 = inverterID;
		gDevice.floatval1 = 0;
		sDecodeRXMessage(this, (const unsigned char *)&gDevice, "Voltage", 255, nullptr);
		while (result.empty()) // wait for device to be created
		{
			sleep_milliseconds(100);
			result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (ltrim(DeviceID,0)='%s')", m_HwdID, pTypeGeneral, sTypeVoltage, szShortID.c_str());
		}
	}
	return result[0][0];
}


std::string CAPSLocalECU::GetWattmeterIDx(const std::string &szShortID, const int channel)
{
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (DeviceID='%s') AND (Unit=%d)", m_HwdID, pTypeUsage, sTypeElectric, szShortID.c_str(), channel + 11);
	if (result.empty())
	{
		_log.Debug(DEBUG_HARDWARE, "ECU: Create watt meter %d for inverter channel %s", channel + 1, szShortID.c_str());
		char *end = NULL;
		int inverterID = (int)strtoul(szShortID.c_str(), &end, 16);
		_tUsageMeter umeter;
		umeter.id1 = 0;
		umeter.id2 = (inverterID >> 16) & 0xFF;
		umeter.id3 = (inverterID >> 8) & 0xFF;
		umeter.id4 = inverterID & 0xFF;
		umeter.dunit = channel + 11;	// voltage sensor already claims unit 1
		umeter.fusage = 0;
		sDecodeRXMessage(this, (const unsigned char *)&umeter, "Power", 255, nullptr);
		while (result.empty()) // wait for device to be created
		{
			sleep_milliseconds(100);
			result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (ltrim(DeviceID,0)='%s')", m_HwdID, pTypeUsage, sTypeElectric, szShortID.c_str());
		}
	}
	return result[0][0];
}


void CAPSLocalECU::SendMeters()
{
	struct tm * tt;
	tt = localtime(&m_ECUClient->m_apsecu.timestamp);
	char timestring[30];
	strftime(timestring, 30, "%Y-%m-%d %H:%M:%S" , tt);

	/* The main counter in the ECU has a 100 Watthour resolution which causes the week graph
	 * to show 1200 Watt spikes when there is little production. To show a somewhat friendlier
	 * graph we use the 10 Watthour resolution from the day counter, however this means that
	 * we need to find the right offset in the internal counter where the day counter started.
	 * We assume this to be where both exported counters have their minimum increment within
	 * the same 5 minute report.
	 */
	unsigned long lifetimeEnergy = m_ECUClient->m_apsecu.lifetime_energy * 1000;
	unsigned int todayEnergy = m_ECUClient->m_apsecu.today_energy * 1000;
	if (todayEnergy == 0)
	{
		uint8_t haveDayLifeSync = m_todayEnergyOffset & 1;
		if (m_lastTodayEnergy > 0)	// new day
		{
			m_lastLifeEnergy = m_usageLow + m_usageHigh;
			m_lastTodayEnergy = 0;
			m_todayEnergyOffset = 0 - (m_lastLifeEnergy % 100) - haveDayLifeSync;
		}
	}
	else if (m_todayEnergyOffset & 1)
	{
		if (lifetimeEnergy > m_lastLifeEnergy)
		{
			int todayEnergyDelta = todayEnergy - m_lastTodayEnergy;
			int lifetimeEnergyDelta = lifetimeEnergy - m_lastLifeEnergy;
			if ((todayEnergyDelta == 10) && (lifetimeEnergyDelta == 100))
				m_todayEnergyOffset = todayEnergy; // set today's offset
			else
				m_todayEnergyOffset = todayEnergy - 1; // set it because it's likely still closer than our previous guess but keep the mark that we still require sync
		}
	}
	m_lastLifeEnergy = lifetimeEnergy;
	m_lastTodayEnergy = todayEnergy;

	if (m_tariff == 0)
		m_usageHigh = lifetimeEnergy + ((todayEnergy - m_todayEnergyOffset) % 100) - m_usageLow;
	else
		m_usageLow = lifetimeEnergy + ((todayEnergy - m_todayEnergyOffset) % 100) - m_usageHigh;

	/*  Not using the standard Rx method here because it does not allow us to use the time from the ECU report
	 */
	char p1data[60];
	sprintf(p1data, "%lu;%lu;0;0;%d;0", m_usageLow, m_usageHigh, m_ECUClient->m_apsecu.current_power);
	std::vector<std::vector<std::string> > result;
	std::string IDx = GetP1IDx();
	result = m_sql.safe_query("UPDATE DeviceStatus SET sValue='%s', lastupdate='%s' WHERE ID=%s", p1data, timestring, IDx.c_str());
	m_mainworker.sOnDeviceReceived(m_HwdID, atoll(IDx.c_str()), "Solar Power", nullptr);

	for (int i = 0; i < m_ECUClient->m_apsecu.inverters.size(); i++)
	{
		std::string szShortID = m_ECUClient->m_apsecu.inverters[i].id.substr(6);
		std::vector<std::vector<std::string> > result;

		IDx = GetVoltmeterIDx(szShortID);
		if (m_ECUClient->m_apsecu.inverters[i].online_status == 0)
			result = m_sql.safe_query("UPDATE DeviceStatus SET DeviceID='%s', sValue='%d', lastupdate='%s' WHERE ID=%s", szShortID.c_str(), 0, timestring, IDx.c_str());
		else
			result = m_sql.safe_query("UPDATE DeviceStatus SET DeviceID='%s', sValue='%d', lastupdate='%s' WHERE ID=%s", szShortID.c_str(), m_ECUClient->m_apsecu.inverters[i].channels[0].volt, timestring, IDx.c_str());
		m_mainworker.sOnDeviceReceived(m_HwdID, atoll(IDx.c_str()), "Voltage", nullptr);

		int numchannels = (int)m_ECUClient->m_apsecu.inverters[i].channels.size();
		if (numchannels == 0)
		{
			result = m_sql.safe_query("SELECT COUNT(*) FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (DeviceID='%s')", m_HwdID, pTypeUsage, sTypeElectric, szShortID.c_str());
			numchannels = atoi(result[0][0].c_str());
		}

		for (int j = 0; j < numchannels; j++)
		{
			IDx = GetWattmeterIDx(szShortID, j);
			if (m_ECUClient->m_apsecu.inverters[i].online_status == 0)
				result = m_sql.safe_query("UPDATE DeviceStatus SET DeviceID='%s', sValue='%d', lastupdate='%s' WHERE ID=%s", szShortID.c_str(), 0, timestring, IDx.c_str());
			else
				result = m_sql.safe_query("UPDATE DeviceStatus SET DeviceID='%s', sValue='%d', lastupdate='%s' WHERE ID=%s", szShortID.c_str(), m_ECUClient->m_apsecu.inverters[i].channels[0].power, timestring, IDx.c_str());
			m_mainworker.sOnDeviceReceived(m_HwdID, atoll(IDx.c_str()), "Power", nullptr);
		}
	}
}

