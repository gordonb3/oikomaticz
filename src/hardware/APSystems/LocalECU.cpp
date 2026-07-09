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

	m_P1IDx = "";
	m_usageLow = 0;
	m_usageHigh = 0;
	m_lastLifeEnergy = 0;
	m_lastTodayEnergy = 0;
	m_lastTensDelta = 100;
	m_todayEnergyOffset = 1;
	m_tariff = 0;
	m_ECUVersion = "";
}


CAPSLocalECU::~CAPSLocalECU(void)
{
	m_bIsStarted=false;
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
			m_ECUClient = new ecuAPI();
			m_ECUClient->SetTargetAddress(m_IPAddress);
			if (GetECUData())
				seconds_remaining = (int)(m_ECULastReport + REPORT_INTERVAL - mytime(NULL));
			delete m_ECUClient;
		}
	}
}


// tariff switch
bool CAPSLocalECU::WriteToHardware(const char *pdata, const unsigned char length)
{
	/* Some of the ECU units expose a web based interface allowing the user to send a limited set
	 * of commands including a reset of the main communication port.
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
	 * (using Zigbee) it will thus reject our communication attempt. The same happens when the ECU is
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
		Debug(DEBUG_HARDWARE, "Proces ECU data");
		m_ECULastReport = m_ECUClient->m_apsecu.timestamp;
		SendMeters();
		// keep track of ECU firmare version to identify possible firmware related issues
		if (m_ECUVersion != m_ECUClient->m_apsecu.version)
		{
			if (m_ECUVersion.empty())
			{
				Log(LOG_STATUS, "ECU firmware version is %s", m_ECUClient->m_apsecu.version.c_str());
			} else {
				Log(LOG_STATUS, "ECU firmware has been updated to version %s", m_ECUClient->m_apsecu.version.c_str());
			}
			m_ECUVersion = m_ECUClient->m_apsecu.version;
		}
		return true;
	}
	if (statuscode == -1)
		Log(LOG_ERROR, "attempt to connect to ECU returned error %d", errno);
	else if (statuscode == 1)
		Debug(DEBUG_HARDWARE, "ECU returned invalid data");
	return false;
}


void CAPSLocalECU::SendMeters()
{
	/* The main counter in the ECU has a 100 Watthour resolution which causes the week graph
	 * to show 1200 Watt spikes when there is little production. To show a somewhat friendlier
	 * graph we use the 10 Watthour resolution from the day counter, however this means that
	 * we need to find the right offset in the internal counter at which tens value the day
	 * counter started. We assume this to be where the lifetime_energy counter is larger and
	 * the tens value of the today_energy counter increments by just 1.
	 */
	unsigned long lifetimeEnergy = m_ECUClient->m_apsecu.lifetime_energy * 1000;
	unsigned int todayEnergy = m_ECUClient->m_apsecu.today_energy * 1000;
	if (todayEnergy == 0)
	{
		if (m_lastTodayEnergy > 0)	// new day
		{
			m_lastTodayEnergy = 0;
			if ((m_usageLow % 10) | (m_usageHigh % 10))
				m_todayEnergyOffset = (m_usageLow - (m_usageLow % 10) + m_usageHigh - (m_usageHigh % 10)) % 100 + 1;
			else
				m_todayEnergyOffset = (m_usageLow + m_usageHigh) % 100;
		}
	}
	else if (m_todayEnergyOffset & 1)
	{
		if (lifetimeEnergy > m_lastLifeEnergy)
		{
			int todayEnergyDelta = todayEnergy - m_lastTodayEnergy;
			int lifetimeEnergyDelta = lifetimeEnergy - m_lastLifeEnergy;
			if (lifetimeEnergyDelta > todayEnergyDelta)
			{
				int tensDelta = todayEnergyDelta % 100;
				if (tensDelta == 10)
					m_todayEnergyOffset = todayEnergy % 100; // set the today_energy counter offset
				else if (tensDelta < m_lastTensDelta)
				{
					m_todayEnergyOffset = (todayEnergy % 100) + 1; // set it because it's still closer than our previous guess but keep the mark that we still require sync
					m_lastTensDelta = tensDelta;
				}
			}
		}
	}
	m_lastLifeEnergy = lifetimeEnergy;
	m_lastTodayEnergy = todayEnergy;

	if (m_tariff == 0)
		m_usageHigh = lifetimeEnergy + ((todayEnergy + m_todayEnergyOffset) % 100) - m_usageLow;
	else
		m_usageLow = lifetimeEnergy + ((todayEnergy + m_todayEnergyOffset) % 100) - m_usageHigh;

	/*  Not using the standard Rx method here because it does not allow us to use the time from the ECU report
	 */
	struct tm * tt;
	tt = localtime(&m_ECUClient->m_apsecu.timestamp);
	char timestring[30];
	strftime(timestring, 30, "%Y-%m-%d %H:%M:%S" , tt);
	char p1data[60];
	sprintf(p1data, "%lu;%lu;0;0;%d;0", m_usageLow, m_usageHigh, m_ECUClient->m_apsecu.current_power);
	std::vector<std::vector<std::string> > result;
	std::string IDx = GetP1IDx();
	if (IDx.empty())
		return; // something is really wrong here
	result = m_sql.safe_query("UPDATE DeviceStatus SET sValue='%s', lastupdate='%s' WHERE ID=%s", p1data, timestring, IDx.c_str());
	uint64_t nIDx = atoll(IDx.c_str());
	// tell mainworker and eventsystem that device was updated
	m_mainworker.sOnDeviceReceived(m_HwdID, nIDx, "Solar Power", nullptr);
	m_mainworker.m_eventsystem.ProcessDevice(m_HwdID, nIDx, 1, pTypeP1Power, sTypeP1Power, 255, 255, 0, p1data, timestring);


	for (int i = 0; i < m_ECUClient->m_apsecu.inverters.size(); i++)
	{
		std::string szShortID = m_ECUClient->m_apsecu.inverters[i].id.substr(6);
		std::vector<std::vector<std::string> > result;

		IDx = GetVoltmeterIDx(szShortID);
		if (!IDx.empty())
		{
			if (m_ECUClient->m_apsecu.inverters[i].online_status == 0)
				result = m_sql.safe_query("UPDATE DeviceStatus SET DeviceID='%s', sValue='%d', lastupdate='%s' WHERE ID=%s", szShortID.c_str(), 0, timestring, IDx.c_str());
			else
				result = m_sql.safe_query("UPDATE DeviceStatus SET DeviceID='%s', sValue='%d', lastupdate='%s' WHERE ID=%s", szShortID.c_str(), m_ECUClient->m_apsecu.inverters[i].channels[0].volt, timestring, IDx.c_str());
			nIDx = atoll(IDx.c_str());
			// tell mainworker and eventsystem that device was updated
			m_mainworker.sOnDeviceReceived(m_HwdID, nIDx, "Voltage", nullptr);
			m_mainworker.m_eventsystem.ProcessDevice(m_HwdID, nIDx, 1, pTypeGeneral, sTypeVoltage, 255, 255, 0, p1data, timestring);
		}

		int numchannels = (int)m_ECUClient->m_apsecu.inverters[i].channels.size();
		if (numchannels == 0)
		{
			result = m_sql.safe_query("SELECT COUNT(*) FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (DeviceID='%s')", m_HwdID, pTypeUsage, sTypeElectric, szShortID.c_str());
			numchannels = atoi(result[0][0].c_str());
		}

		for (int j = 0; j < numchannels; j++)
		{
			IDx = GetWattmeterIDx(szShortID, j);
			if (!IDx.empty())
			{
				if (m_ECUClient->m_apsecu.inverters[i].online_status == 0)
					result = m_sql.safe_query("UPDATE DeviceStatus SET DeviceID='%s', sValue='%d', lastupdate='%s' WHERE ID=%s", szShortID.c_str(), 0, timestring, IDx.c_str());
				else
					result = m_sql.safe_query("UPDATE DeviceStatus SET DeviceID='%s', sValue='%d', lastupdate='%s' WHERE ID=%s", szShortID.c_str(), m_ECUClient->m_apsecu.inverters[i].channels[j].power, timestring, IDx.c_str());
				nIDx = atoll(IDx.c_str());
				// tell mainworker and eventsystem that device was updated
				m_mainworker.sOnDeviceReceived(m_HwdID, nIDx, "Power", nullptr);
				m_mainworker.m_eventsystem.ProcessDevice(m_HwdID, nIDx, j + 11, pTypeUsage, sTypeElectric, 255, 255, 0, p1data, timestring);
			}
		}
	}
}


void CAPSLocalECU::Init()
{
	// retrieve the current tariff setting
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
			sleep_milliseconds(100);
			i--;
			result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (DeviceID='FFFFFFFF') AND (Unit=1) AND (Type=%d) AND (SubType=%d)", m_HwdID, pTypeGeneralSwitch, sSwitchGeneralSwitch);
		}

		// set SwitchType and CustomImage
		if (!result.empty())
		{
			int iconID = CUSTOM_IMAGE_ID;
			m_sql.safe_query("UPDATE DeviceStatus SET SwitchType=%d, CustomImage=%d WHERE (HardwareID=%d) AND (DeviceID='FFFFFFFF') AND (Unit=1) AND (Type=%d) AND (SubType=%d)", int(device::tswitch::type::OnOff), iconID, m_HwdID, pTypeGeneralSwitch, sSwitchGeneralSwitch);
		}
	}
	else
	{
		m_tariff = atoi(result[0][0].c_str());
	}

	// retrieve last known status of the P1 device
	std::string szLastUpdate = "";
	result = m_sql.safe_query("SELECT ID,sValue,LastUpdate FROM DeviceStatus WHERE (HardwareID=%d) AND (Type=%d) AND (Subtype='%d')", m_HwdID, pTypeP1Power, sTypeP1Power);
	if (!result.empty())
	{
		for (const auto &sd : result)
		{
			m_P1IDx = sd[0];
			std::string sValue = sd[1];
			std::vector<std::string> splitresults;
			StringSplit(sValue, ";", splitresults);
			m_usageLow = std::strtoul(splitresults[0].c_str(), nullptr, 10);
			m_usageHigh = std::strtoul(splitresults[1].c_str(), nullptr, 10);
			szLastUpdate = sd[2];

			if (m_usageLow % 10)
				m_usageLow = m_usageLow - (m_usageLow % 10);
			if (m_usageHigh % 10)
				m_usageHigh = m_usageHigh - (m_usageHigh % 10);
		}
	}

	// use today's night time values to determine the offset to apply for ECU's today_energy counter (used by SendMeters())
	if (!m_P1IDx.empty())
	{
		char cToday[40];
		time_t now = mytime(nullptr);
		struct tm tm1;
		localtime_r(&now, &tm1);
		sprintf(cToday, "%04d-%02d-%02d", tm1.tm_year + 1900, tm1.tm_mon + 1, tm1.tm_mday);
		std::string szToday = cToday;
		if (szLastUpdate.compare(szToday) > 0)
		{
			result = m_sql.safe_query("SELECT MIN(Value1), MIN(Value5) FROM MultiMeter WHERE (DeviceRowID='%q' AND Date>='%q 00:00:00'  AND Date<='%q 03:00:00')", m_P1IDx.c_str(), cToday, cToday);
			if (!result.empty())
			{
				std::vector<std::string> sd = result[0];
				uint64_t nightUsageLow = std::stoull(sd[0]);
				uint64_t nightUsageHigh = std::stoull(sd[1]);
				if ((nightUsageLow % 10) | (nightUsageHigh % 10))
					m_todayEnergyOffset = (nightUsageLow - (nightUsageLow % 10) + nightUsageHigh - (nightUsageHigh % 10)) % 100 + 1;
				else
					m_todayEnergyOffset = (nightUsageLow + nightUsageHigh) % 100;
			}
		}
	}

}


std::string CAPSLocalECU::GetP1IDx()
{
	if (!m_P1IDx.empty())
		return m_P1IDx;
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d')", m_HwdID, pTypeP1Power, sTypeP1Power);
	if (result.empty())
	{
		Debug(DEBUG_HARDWARE, "ECU: Create solar power meter");
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

		// wait a maximum of 1 second for mainworker to finish adding the device
		int i = 10;
		while (i && result.empty())
		{
			sleep_milliseconds(100);
			i--;
			result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d')", m_HwdID, pTypeP1Power, sTypeP1Power);
		}
	}
	if (!result.empty())
		m_P1IDx = result[0][0];
	return m_P1IDx;
}


std::string CAPSLocalECU::GetVoltmeterIDx(const std::string &szShortID)
{
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (DeviceID='%s')", m_HwdID, pTypeGeneral, sTypeVoltage, szShortID.c_str());
	if (result.empty())
	{
		Debug(DEBUG_HARDWARE, "ECU: Create voltage meter for inverter %s", szShortID.c_str());
		char *end = NULL;
		int inverterID = (int)strtoul(szShortID.c_str(), &end, 16);
		_tGeneralDevice gDevice;
		gDevice.subtype = sTypeVoltage;
		gDevice.id = 0;
		gDevice.intval1 = inverterID;
		gDevice.floatval1 = 0;
		sDecodeRXMessage(this, (const unsigned char *)&gDevice, "Voltage", 255, nullptr);

		// wait a maximum of 1 second for mainworker to finish adding the device
		int i = 10;
		while (i && result.empty())
		{
			sleep_milliseconds(100);
			i--;
			result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (ltrim(DeviceID,0)='%s')", m_HwdID, pTypeGeneral, sTypeVoltage, szShortID.c_str());
		}
	}
	if (!result.empty())
		return result[0][0];
	return "";
}


std::string CAPSLocalECU::GetWattmeterIDx(const std::string &szShortID, const int channel)
{
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (DeviceID='%s') AND (Unit=%d)", m_HwdID, pTypeUsage, sTypeElectric, szShortID.c_str(), channel + 11);
	if (result.empty())
	{
		Debug(DEBUG_HARDWARE, "ECU: Create watt meter %d for inverter channel %s", channel + 1, szShortID.c_str());
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

		// wait a maximum of 1 second for mainworker to finish adding the device
		int i = 10;
		while (i && result.empty())
		{
			sleep_milliseconds(100);
			i--;
			result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (Type='%d') AND (Subtype='%d') AND (ltrim(DeviceID,0)='%s')", m_HwdID, pTypeUsage, sTypeElectric, szShortID.c_str());
		}
	}
	if (!result.empty())
		return result[0][0];
	return "";
}

