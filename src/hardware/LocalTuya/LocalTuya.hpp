/*
 * Local Tuya provider for Oikomaticz
 *
 *  Copyright 2017 - gordonb3 https://github.com/gordonb3/tuyapp
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 * @license GPL-3.0+ <https://github.com/gordonb3/tuyapp/blob/master/LICENSE>
 *
 *
 *
 *
 */

#pragma once
#include "hardware/DomoticzHardware.h"
#include "TuyaMonitor.hpp"


class CLocalTuya : public CDomoticzHardwareBase
{
public:
	CLocalTuya(int ID);
	~CLocalTuya(void) override;
	bool WriteToHardware(const char *pdata, const unsigned char length) override;

private:
	std::shared_ptr<std::thread> m_thread;
	uint8_t m_tariff;

	void Init();
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();
	void SendSwitch(const TuyaData* devicedata);
	void SendMeter(const TuyaData* devicedata);

};


