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

#pragma once
#include "hardware/DomoticzHardware.h"
#include "ecuAPI.hpp"
#include <time.h>



class CAPSLocalECU : public CDomoticzHardwareBase
{
public:
	CAPSLocalECU(int ID, const std::string &IPAddress);
	~CAPSLocalECU(void) override;

	bool WriteToHardware(const char *pdata, const unsigned char length) override;

private:
	std::shared_ptr<std::thread> m_thread;
	std::string m_IPAddress;
	uint8_t m_tariff;
	unsigned long m_usageLow;
	unsigned long m_usageHigh;
	ecuAPI *m_ECUClient;
	time_t m_ECULastReport;

	void Init();
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();

	bool GetECUData();
	void SendMeters();

	std::string GetP1IDx();
	std::string GetVoltmeterIDx(const std::string &szShortID);
	std::string GetWattmeterIDx(const std::string &szShortID, const int channel);
};

