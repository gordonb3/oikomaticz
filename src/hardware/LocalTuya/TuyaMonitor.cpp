#define TUYA_COMMAND_PORT 6668


#include "TuyaMonitor.hpp"
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <cmath>
#include "main/json_helper.h"

#include <fstream>


TuyaMonitor::TuyaMonitor(const unsigned int seqnr, const std::string &name, const std::string &id, const std::string &key, const std::string &address, const int energyDivider) :
	m_name(name),
	m_id(id),
	m_key(key),
	m_address(address)
{
	m_devicedata = new TuyaData();
	memset(m_devicedata, 0, sizeof(TuyaData));
	m_devicedata->deviceID = seqnr;
	m_devicedata->energyDivider = energyDivider;
	strncpy(m_devicedata->deviceName, m_name.c_str(), 19);

	m_tuyaclient = new tuyaAPI33();

	m_isPowerMeter = false;
}

TuyaMonitor::~TuyaMonitor()
{
	delete m_tuyaclient;
}


bool TuyaMonitor::ConnectToDevice()
{
	if (!m_tuyaclient->ConnectToDevice(m_address, TUYA_COMMAND_PORT))
		return false;

	// request current state of the device
	std::stringstream ss_payload;
	long currenttime = time(NULL) ;
	ss_payload << "{\"gwId\":\"" << m_id << "\",\"devId\":\"" << m_id << "\",\"uid\":\"" << m_id << "\",\"t\":\"" << currenttime << "\"}";
	std::string payload = ss_payload.str();

	int numbytes = m_tuyaclient->BuildTuyaMessage(message_buffer, TUYA_DP_QUERY, payload, m_key);
	numbytes = m_tuyaclient->send(message_buffer, numbytes);
	if (numbytes < 0)
		return false;
	numbytes = m_tuyaclient->receive(message_buffer, MAX_BUFFER_SIZE - 1);
	if (numbytes < 0)
		return false;
	std::string tuyaresponse = m_tuyaclient->DecodeTuyaMessage(message_buffer, numbytes, m_key);

	Json::Value jStatus;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	jReader->parse(tuyaresponse.c_str(), tuyaresponse.c_str() + tuyaresponse.size(), &jStatus, nullptr);
	if (!jStatus.isMember("dps"))
		return false;

	if (jStatus["dps"].isMember("1"))
	{
		m_devicedata->switchstate = jStatus["dps"]["1"].asBool();
		sigSendSwitch(m_devicedata);
	}
	if (jStatus["dps"].isMember("19"))
	{
		m_devicedata->power = jStatus["dps"]["19"].asUInt();
		m_isPowerMeter = true;
	}
	return true;
}

bool TuyaMonitor::StartMonitor()
{
	if (!ConnectToDevice())
		return false;
	RequestStart();
	m_thread = std::make_shared<std::thread>([this] { MonitorThread(); });
	if (!m_thread)
		return false;
	m_devicedata->connected = true;
	return true;
}


bool TuyaMonitor::StopMonitor()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	return true;
}


void TuyaMonitor::MonitorThread()
{
	unsigned long timeval = 0;
	std::string payload;

	int numbytes;

	Json::Value jStatus;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());

	while (!IsStopRequested(1))
	{

		if (m_isPowerMeter)
		{

			// request data point updates
			payload = "{\"dpId\":[19]}";
			numbytes = m_tuyaclient->BuildTuyaMessage(message_buffer, TUYA_UPDATEDPS, payload, m_key);
		}
		else
		{
			// send heart beat to keep connection alive
			payload = "{\"gwId\":\"" + m_id + "\",\"devId\":\"" + m_id + "\"}";
			numbytes = m_tuyaclient->BuildTuyaMessage(message_buffer, TUYA_HEART_BEAT, payload, m_key);
		}

		numbytes = m_tuyaclient->send(message_buffer, numbytes);
		if (numbytes < 0)
			break;

		numbytes = m_tuyaclient->receive(message_buffer, MAX_BUFFER_SIZE - 1);
		if (numbytes < 0)
		{
			// expect a timeout because the device will only send updates when the requested values change
			if (errno != 11)
				break;
		}
		else
		{
			std::string tuyaresponse = m_tuyaclient->DecodeTuyaMessage(message_buffer, numbytes, m_key);

			jReader->parse(tuyaresponse.c_str(), tuyaresponse.c_str() + tuyaresponse.size(), &jStatus, nullptr);
			if (jStatus.isMember("dps"))
			{
				if (jStatus["dps"].isMember("1"))
				{
					m_devicedata->switchstate = jStatus["dps"]["1"].asBool();
					sigSendSwitch(m_devicedata);
				}

				if (jStatus["dps"].isMember("19"))
				{
					unsigned long newtimeval = jStatus["t"].asUInt64();
					if (timeval && jStatus["dps"].isMember("19"))
					{
						unsigned int timediff = (int)(newtimeval - timeval);
						m_devicedata->power = jStatus["dps"]["19"].asUInt();
						if (m_devicedata->isLowTariff)
							m_devicedata->usageLow += (m_devicedata->power * timediff / 3600.0);
						else
							m_devicedata->usageHigh += (m_devicedata->power * timediff / 3600.0);
						sigSendMeter(m_devicedata);
					}
					timeval = newtimeval;
				}
			}
		}
	}

	// inform main that thread has ended
	m_devicedata->connected = false;
}


bool TuyaMonitor::SendCommand()
{
	return true;
}


void TuyaMonitor::SetMeterStartData(const float usageHigh, const float usageLow)
{
	m_devicedata->usageLow = usageLow;
	m_devicedata->usageHigh = usageHigh;
}

