/**
 * client interface for UK/EMEA Evohome API
 *
 *  Copyright 2017-2020 - gordonb3 https://github.com/gordonb3/evohomeclient
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 *  @license GPL-3.0+ <https://github.com/gordonb3/evohomeclient/blob/master/LICENSE>
 */
#pragma once

#include "EvohomeBase.h"

#include "evohomeclient/src/common/devices.hpp"
#include "main/json_helper.h"

class CEvohomeWeb : public CEvohomeBase
{
public:
	CEvohomeWeb(const int ID, const std::string &Username, const std::string &Password, const unsigned int refreshrate, const int UseFlags, const unsigned int installation);
	~CEvohomeWeb(void);
	bool WriteToHardware(const char *pdata, const unsigned char length) override;

private:
	// base functions
	void Init();
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();

	// evohome web commands
	bool StartSession();
	bool GetStatus();
	bool SetSystemMode(uint8_t sysmode);
	bool SetSetpoint(const char *pdata);
	bool SetDHWState(const char *pdata);

	// status readers
	void DecodeControllerMode(evohome::device::temperatureControlSystem* tcs);
	void DecodeDHWState(evohome::device::temperatureControlSystem* tcs);
	void DecodeZone(evohome::device::zone* hz);

	// database helpers
	uint8_t GetUnit_by_ID(unsigned long evoID);

	// logging helpers
	int GetLastV2ResponseCode();


private:
	std::shared_ptr<std::thread> m_thread;

	// settings
	std::string m_username;
	std::string m_password;
	bool m_updatedev;
	bool m_showSchedule;
	bool m_showLocation;
	bool m_showhdtemps;
	uint8_t m_hdprecision;
	unsigned int m_refreshRate;
	uint8_t m_locationIdx;
	uint8_t m_gatewayIdx;
	uint8_t m_systemIdx;

	// flow states
	bool m_loggedon;
	unsigned int m_logonfailures;
	unsigned int m_lastAccessTimer;

	// translation maps
	static const uint8_t m_dczToEvoWebAPIMode[7];
	static const std::string weekdays[7];

	// evohome data
	std::string m_szLocationName;
	double m_awaysetpoint;
	uint8_t m_wdayoff;

	// verbosity level (on/off)
	bool m_beMoreVerbose;

	std::vector<evohome::device::location> m_locations;
	std::vector<std::vector<unsigned long>> m_vUnits;
};

