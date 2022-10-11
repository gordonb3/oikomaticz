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

#define TUYA_COMMAND_PORT 6668

#include "TuyaMonitor.hpp"
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <cmath>
#include "main/json_helper.h"
#include <chrono>
#include "main/Logger.h"

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

	m_isPowerMeter = false;
}


TuyaMonitor::~TuyaMonitor()
{
	StopMonitor();
	delete m_devicedata;
}


bool TuyaMonitor::ConnectToDevice()
{
	m_tuyaclient = new tuyaAPI33();
	if (!m_tuyaclient->ConnectToDevice(m_address, TUYA_COMMAND_PORT, 1))
	{
		_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: failed to connect to %s - wrong IP?", m_name.c_str());
		return false;
	}

	// request current state of the device
	std::stringstream ss_payload;
	long currenttime = time(NULL) ;
	ss_payload << "{\"gwId\":\"" << m_id << "\",\"devId\":\"" << m_id << "\",\"uid\":\"" << m_id << "\",\"t\":\"" << currenttime << "\"}";
	std::string payload = ss_payload.str();

	int numbytes = m_tuyaclient->BuildTuyaMessage(message_buffer, TUYA_DP_QUERY, payload, m_key);
	numbytes = m_tuyaclient->send(message_buffer, numbytes);
	if (numbytes < 0)
	{
		_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: failed send to %s", m_name.c_str());
		return false;
	}
	numbytes = m_tuyaclient->receive(message_buffer, MAX_BUFFER_SIZE - 1);
	if (numbytes < 0)
	{
		_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: failed receive from %s, error is %d", m_name.c_str(), errno);
		return false;
	}
	std::string tuyaresponse = m_tuyaclient->DecodeTuyaMessage(message_buffer, numbytes, m_key);

	Json::Value jStatus;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	jReader->parse(tuyaresponse.c_str(), tuyaresponse.c_str() + tuyaresponse.size(), &jStatus, nullptr);
	if (!jStatus.isMember("dps"))
	{
		_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: received invalid data from %s, verify ID and local key", m_name.c_str());
		return false;
	}

	if (jStatus["dps"].isMember("1"))
	{
		m_devicedata->switchstate = jStatus["dps"]["1"].asBool();
		sigSendSwitch(m_devicedata);
	}
	if ((m_devicedata->energyDivider != 0) && jStatus["dps"].isMember("19"))
	{
		m_devicedata->power = jStatus["dps"]["19"].asUInt();
		m_isPowerMeter = true;
	}
	return true;
}


bool TuyaMonitor::StartMonitor()
{
	if (m_devicedata->connectstate == device::tuya::connectstate::OFFLINE)
	{
		m_devicedata->connectstate = device::tuya::connectstate::STARTING;
		if (ConnectToDevice())
		{
			RequestStart();
			m_thread = std::make_shared<std::thread>([this] { MonitorThread(); });
			m_devicedata->connectstate = device::tuya::connectstate::CONNECTED;
			return true;
		}
		if (m_tuyaclient != nullptr)
		{
			delete m_tuyaclient;
			m_tuyaclient = nullptr;
		}
		m_devicedata->connectstate = device::tuya::connectstate::OFFLINE;
	}
	return false;
}


bool TuyaMonitor::StopMonitor()
{
	while (m_devicedata->connectstate == device::tuya::connectstate::STARTING)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
	m_devicedata->connectstate = device::tuya::connectstate::STOPPING;
	if (m_thread != nullptr)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	if (m_tuyaclient != nullptr)
	{
		delete m_tuyaclient;
		m_tuyaclient = nullptr;
	}
	m_devicedata->connectstate = device::tuya::connectstate::STOPPED;
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

	while (!IsStopRequested(1) && (m_devicedata->connectstate == device::tuya::connectstate::CONNECTED))
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
		{
			_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: Lost communication with %s", m_name.c_str());
			m_devicedata->connectstate = device::tuya::connectstate::RESETBYPEER;
			break;
		}

		numbytes = m_tuyaclient->receive(message_buffer, MAX_BUFFER_SIZE - 1);
		if (numbytes < 0)
		{
			// expect a timeout because the device will only send updates when the requested values change
			if (errno != 11)
			{
				_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: device %s returned error %d", m_name.c_str(), errno);
				m_devicedata->connectstate = device::tuya::connectstate::RESETBYPEER;
				break;
			}
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
					if (m_waitForSwitch)
						m_waitForSwitch = false;
					else
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

	while (!IsStopRequested(1))
	{
		// wait for final stop signal to end the thread
	}
}


bool TuyaMonitor::SendSwitchCommand(int switchstate)
{
	long currenttime = time(NULL) ;
	std::stringstream ss_payload;
	ss_payload << "{\"devId\":\"" << m_id << "\",\"uid\":\"" << m_id << "\",\"dps\":{\"1\":";
	if (switchstate)
		ss_payload << "true";
	else
		ss_payload << "false";
	ss_payload <<  "},\"t\":\"" << currenttime << "\"}";
	std::string payload = ss_payload.str();
	int numbytes = m_tuyaclient->BuildTuyaMessage(message_buffer, TUYA_CONTROL, payload, m_key);

	m_waitForSwitch = true;
	numbytes = m_tuyaclient->send(message_buffer, numbytes);
	if (numbytes < 0)
		return false;
	for (int i = 0; i < 20; i++)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		if (!m_waitForSwitch)
			return true;
	}
	m_waitForSwitch = false;
	return false;
}


void TuyaMonitor::SetMeterStartData(const float usageHigh, const float usageLow)
{
	m_devicedata->usageLow = usageLow;
	m_devicedata->usageHigh = usageHigh;
}
