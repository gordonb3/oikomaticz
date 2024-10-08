/*
 *  Monitoring loop for the local Tuya client
 *
 *  Copyright 2022 - gordonb3 https://github.com/gordonb3/tuyapp
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 *  @license GPL-3.0+ <https://github.com/gordonb3/tuyapp/blob/master/LICENSE>
 */

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 1024
#endif

#include "tuyaAPI33.hpp"
#include <string>
#include <thread>
#include <mutex>
#include <memory>
#include <boost/signals2.hpp>
#include "main/StoppableTask.h"

namespace device {
namespace tuya {
namespace connectstate {
	enum value {
		OFFLINE = 0,
		STARTING,
		STOPPING,
		CONNECTED,
		RESETBYPEER,
		STOPPED
	};
}; // namespace connectstate
namespace protocolversion {
	enum value {
		v33 = 0,
		v33_1
	};
}; // namespace protocolversion
}; // namespace tuya
}; // namespace device


class TuyaData
{
public:
	unsigned int deviceID;
	char deviceName[20];
	device::tuya::connectstate::value connectstate;
	bool switchstate;
	bool isLowTariff;
	unsigned int power;
	int energyDivider;
	unsigned int voltage;
	double usageLow;
	double usageHigh;
};


class TuyaMonitor : public StoppableTask
{

public:
	TuyaMonitor(const unsigned int seqnr, const std::string &name, const std::string &id, const std::string &key, const std::string &address, const device::tuya::protocolversion::value eProtocolVersion, const int energyDivider);
	~TuyaMonitor();

	bool StartMonitor();
	bool StopMonitor();
	bool SendSwitchCommand(int switchstate);
	void SetMeterStartData(const double usageHigh, const double usageLow);

	TuyaData* m_devicedata;
	boost::signals2::signal<void(TuyaData* tuyadevice)> sigSendSwitch;
	boost::signals2::signal<void(TuyaData* tuyadevice)> sigSendMeter;
	boost::signals2::signal<void(TuyaData* tuyadevice)> sigSendVoltage;

private:

	bool ConnectToDevice();
	void MonitorThread();

	tuyaAPI33 *m_tuyaclient;
	std::string m_name, m_id, m_key, m_address;
	device::tuya::protocolversion::value m_protocolversion;
	unsigned char message_buffer[MAX_BUFFER_SIZE];
	std::shared_ptr<std::thread> m_thread;
	bool m_isPowerMeter;
	bool m_waitForSwitch;
};

