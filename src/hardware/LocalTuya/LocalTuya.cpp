/*
 * Local Tuya provider for Oikomaticz
 *
 *  Copyright 2017 - gordonb3 https://github.com/gordonb3/tuyapp
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

#include <string>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>


extern std::string szUserDataFolder;


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

}


bool CLocalTuya::StartHardware()
{
	RequestStart();

	Init();
	m_thread = std::make_shared<std::thread>([this] { Do_Work(); });
	SetThreadNameInt(m_thread->native_handle());
	if (!m_thread)
		return false;
	m_bIsStarted = true;
	sOnConnected(this);
	return true;
}


bool CLocalTuya::StopHardware()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	m_bIsStarted = false;
	return true;
}


void CLocalTuya::Do_Work()
{
	// connect to devices

	unsigned long sec_counter = 0;
	while (!IsStopRequested(1000))
	{
		sec_counter++;
		if (sec_counter % 12 == 0) {
			m_LastHeartbeat = mytime(NULL);
		}

		// track disconnected devices

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
			m_tariff = 1;
		else
			m_tariff = 0; // high/single tariff
		return true;
	}

//	char szDeviceID[20];
//	sprintf(szDeviceID, "%08X", pSwitch->id);

	// send switch command to device communication thread

	return true;
}


void CLocalTuya::SendMeter(const TuyaData* devicedata)
{
	if ((devicedata->usageHigh > 0) || (devicedata->usageLow > 0))
	{
		P1Power	tuya_energy;
		memset(&tuya_energy, 0, sizeof(P1Power));
		tuya_energy.len = sizeof(P1Power) - 1;
		tuya_energy.type = pTypeP1Power;
		tuya_energy.subtype = sTypeP1Power;
		tuya_energy.ID = devicedata->deviceID;

		if (devicedata->isEnergyInput)
		{
			tuya_energy.delivcurrent = devicedata->power;
			tuya_energy.powerdeliv1 = devicedata->usageLow;
			tuya_energy.powerdeliv2 = devicedata->usageHigh;
		}
		else
		{
			tuya_energy.usagecurrent = devicedata->power;
			tuya_energy.powerusage1 = devicedata->usageLow;
			tuya_energy.powerusage2 = devicedata->usageHigh;
		}
		sDecodeRXMessage(this, (const unsigned char *)&tuya_energy, devicedata->deviceName, 255, nullptr);
	}
}

void CLocalTuya::SendSwitch(const TuyaData* devicedata)
{
	GeneralSwitch tuya_switch;
	memset(&tuya_switch, 0, sizeof(GeneralSwitch));
	tuya_switch.len = sizeof(GeneralSwitch) - 1;
	tuya_switch.type = pTypeGeneralSwitch;
	tuya_switch.subtype = sSwitchTypeAC;
	tuya_switch.switchtype = device::tswitch::type::OnOff;
	tuya_switch.id = devicedata->deviceID;
	tuya_switch.unitcode = 2; // P1 meter claims unit code 1
	tuya_switch.cmnd = (uint8_t)devicedata->switchstate;
	sDecodeRXMessage(this, (const unsigned char *)&tuya_switch, devicedata->deviceName, 255, nullptr);
}
