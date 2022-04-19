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


class TuyaData
{
public:
	unsigned int deviceID;
	char deviceName[20];
	bool isEnergyInput;
	bool connected;
	bool switchstate;
	bool isLowTariff;
	unsigned int power;
	float usageLow;
	float usageHigh;
};


class TuyaMonitor : public StoppableTask
{

public:
	TuyaMonitor(const unsigned int seqnr, const std::string &name, const std::string &id, const std::string &key, const std::string &address);
	~TuyaMonitor();

	bool StartMonitor();
	bool StopMonitor();
	bool SendCommand();

	TuyaData* m_devicedata;
	boost::signals2::signal<void(TuyaData* tuyadevice)> sigSendSwitch;
	boost::signals2::signal<void(TuyaData* tuyadevice)> sigSendMeter;

private:

	bool ConnectToDevice();
	void MonitorThread();

	tuyaAPI33 *m_tuyaclient;
	std::string m_name, m_id, m_key, m_address;
	unsigned char message_buffer[MAX_BUFFER_SIZE];
	std::shared_ptr<std::thread> m_thread;
	bool m_isPowerMeter;
};


