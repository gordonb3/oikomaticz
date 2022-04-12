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
	_log.Log(LOG_STATUS, "Worker started...");
	unsigned long sec_counter = 0;
	while (!IsStopRequested(1000))
	{
		sleep_seconds(1);
		sec_counter++;
		if (sec_counter % 10 == 0) {
			m_LastHeartbeat = mytime(NULL);
		}
		if (sec_counter % 60 == 0)
		{
//			GetStatus();
		}
	}
	_log.Log(LOG_STATUS,"Worker stopped...");

}


bool CLocalTuya::WriteToHardware(const char *pdata, const unsigned char length)
{
	if(!pdata)
		return false;

	// switch command
	const _tGeneralSwitch *pSwitch = reinterpret_cast<const _tGeneralSwitch*>(pdata);
	if (pSwitch->type != pTypeGeneralSwitch)
		return false; //only allowed to control regular switches

	int cmnd = pSwitch->cmnd;

	if (pSwitch->id == 255)
	{
		// tariff indicator
		if (cmnd == gswitch_sOn)
			m_tariff = 1;
		else
			m_tariff = 0;
		
	}
	char szDeviceID[20];
	sprintf(szDeviceID, "%08X", pSwitch->id);


	return true;
}



