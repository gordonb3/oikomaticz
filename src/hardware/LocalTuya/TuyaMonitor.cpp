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

#ifdef WIN32
 #include <Winsock2.h>
#endif

TuyaMonitor::TuyaMonitor(const unsigned int seqnr, const std::string &name, const std::string &id, const std::string &key, const std::string &address, const std::string protocolVersion, const int energyDivider) :
	m_name(name),
	m_id(id),
	m_key(key),
	m_address(address),
	m_protocolversion(protocolVersion)
{
	m_devicedata = new TuyaData();
	memset(m_devicedata, 0, sizeof(TuyaData));
	m_devicedata->deviceID = seqnr;
	m_devicedata->energyDivider = energyDivider;
	strncpy(m_devicedata->deviceName, m_name.c_str(), 19);

	m_isPowerMeter = false;
	m_sendNOOPonConnect = false;
}


TuyaMonitor::~TuyaMonitor()
{
	StopMonitor();
	delete m_devicedata;
}


bool TuyaMonitor::ConnectToDevice()
{
	if (m_protocolversion == "3.3.1")
		m_tuyaclient = tuyaAPI::create("3.3");
	else
		m_tuyaclient = tuyaAPI::create(m_protocolversion);

	m_tuyaclient->setAsyncMode();

	int i = 0;
	m_tuyaclient->ConnectToDevice(m_address);
	while (!m_tuyaclient->isSocketWritable() && (i < 500) && (!IsStopRequested(10))) // 5 seconds
	{
#ifdef WIN32
		if (m_tuyaclient->getlasterror() != WSAEWOULDBLOCK)
			break;
#else
		if ((m_tuyaclient->getlasterror() != EAGAIN) && (m_tuyaclient->getlasterror() != EINPROGRESS))
			break;
#endif
		i++;
	}

	if (m_tuyaclient->getlasterror() != 0)
	{
		_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: failed to connect to %s, %s", m_name.c_str(), strerror(m_tuyaclient->getlasterror()));
		return false;
	}

	// request current state of the device
	std::stringstream ss_payload;
	long currenttime;
	std::string payload;
	int payload_len;
	int numbytes;
	if (m_sendNOOPonConnect)
	{
		m_sendNOOPonConnect = false;
		ss_payload.str("");
		currenttime = time(NULL);
		ss_payload << "{\"devId\":\"" << m_id << "\",\"uid\":\"" << m_id << "\",\"dps\":{\"9\":0" << "},\"t\":\"" << currenttime << "\"}";
		payload = ss_payload.str();
		if ((m_protocolversion == "3.1") || (m_protocolversion == "3.3"))
			payload_len = m_tuyaclient->BuildTuyaMessage(m_cMessageBuffer, TUYA_DP_QUERY, payload, m_key);
		else
			payload_len = m_tuyaclient->BuildTuyaMessage(m_cMessageBuffer, TUYA_CONTROL_NEW, payload, m_key);
		numbytes = m_tuyaclient->send(m_cMessageBuffer, payload_len);
		numbytes = ReadFromDevice(2);
		if (numbytes <= 0)
		{
			_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: NOOP command failed to free device %s, error is %d", m_name.c_str(), m_tuyaclient->getlasterror());
			return false;
		}
	}
	ss_payload.str("");
	currenttime = time(NULL);
	ss_payload << "{\"gwId\":\"" << m_id << "\",\"devId\":\"" << m_id << "\",\"uid\":\"" << m_id << "\",\"t\":\"" << currenttime << "\",\"dps\":{\"1\":null,\"9\":null,\"18\":null,\"19\":null,\"20\":null}}";
	payload = ss_payload.str();

	if ((m_protocolversion == "3.1") || (m_protocolversion == "3.3"))
		payload_len = m_tuyaclient->BuildTuyaMessage(m_cMessageBuffer, TUYA_DP_QUERY, payload, m_key);
	else
		payload_len = m_tuyaclient->BuildTuyaMessage(m_cMessageBuffer, TUYA_CONTROL_NEW, payload, m_key);

	numbytes = m_tuyaclient->send(m_cMessageBuffer, payload_len);

	memset(m_cMessageBuffer, 0, MAX_BUFFER_SIZE);
	numbytes = ReadFromDevice(2);
	if (numbytes <= 0)
	{
		_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: device %s returned error %d on status request", m_name.c_str(), m_tuyaclient->getlasterror());
		if (m_tuyaclient->getlasterror() > 0)
			m_sendNOOPonConnect = true;
		return false;
	}
	std::string tuyaresponse = m_tuyaclient->DecodeTuyaMessage(m_cMessageBuffer, numbytes, m_key);

	Json::Value jStatus;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	jReader->parse(tuyaresponse.c_str(), tuyaresponse.c_str() + tuyaresponse.size(), &jStatus, nullptr);
	if (!jStatus.isMember("dps"))
	{
		_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: received invalid data from %s, verify ID and local key", m_name.c_str());
		if (jStatus.isMember("msg"))
			_log.Log(LOG_ERROR, "Tuya Monitor: client returned \"%s\"", jStatus["msg"].asString().c_str());
		return false;
	}

	if (jStatus["dps"].isMember("1"))
	{
		m_devicedata->switchstate = jStatus["dps"]["1"].asBool();
		sigSendSwitch(m_devicedata);
	}
	if (m_devicedata->energyDivider != 0)
	{
		if (jStatus["dps"].isMember("19"))
		{
			m_devicedata->power = jStatus["dps"]["19"].asUInt();
			m_isPowerMeter = true;
		}
		if (jStatus["dps"].isMember("20"))
		{
			m_devicedata->voltage = jStatus["dps"]["20"].asUInt();
			sigSendVoltage(m_devicedata);
		}
	}
	return true;
}


bool TuyaMonitor::StartMonitor()
{
	if (m_devicedata->connectstate == device::tuya::connectstate::OFFLINE)
	{
		RequestStart();
		m_devicedata->connectstate = device::tuya::connectstate::STARTING;
		if (ConnectToDevice())
		{
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
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
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


int TuyaMonitor::SendToDevice(const int numbytes)
{
	int result = m_tuyaclient->send(m_cMessageBuffer, numbytes);
	if (result <= 0)
	{
#ifdef WIN32
		if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
		if (errno == EAGAIN)
#endif
		{
			_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: send to device %s was delayed", m_name.c_str());
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			int so_error = m_tuyaclient->getlasterror();
			if (so_error == 0)
				return 1;
		}
	}
	return result;
}


int TuyaMonitor::ReadFromDevice(const int timeout)
{
	int numbytes = -1;
	int i = 0;
	while ((numbytes <= 28) && (i < (timeout * 100)) && (!IsStopRequested(10)))
	{
		i++;
		numbytes = m_tuyaclient->receive(m_cMessageBuffer, MAX_BUFFER_SIZE - 1);
		if (numbytes < 0)
		{
			// expect a timeout because the device will only respond to UPDATEDPS when the requested values change
#ifdef WIN32
			if (WSAGetLastError() == WSAEWOULDBLOCK)
				continue;
#else
			if ((errno == EAGAIN) || (errno == EINPROGRESS))
				continue;
#endif
			_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: device %s returned error %d on read", m_name.c_str(), m_tuyaclient->getlasterror());
			m_devicedata->connectstate = device::tuya::connectstate::RESETBYPEER;
			break;
		}

		if (numbytes <= 28)
		{
			// device sent us a message with an empty payload - wait for one that does contain an actual payload
			continue;
		}
	}
	if ((m_tuyaclient->getlasterror() != 0) && (m_tuyaclient->getlasterror() != EAGAIN) && (m_tuyaclient->getlasterror() != EINPROGRESS))
	{
		m_devicedata->connectstate = device::tuya::connectstate::RESETBYPEER;
		_log.Debug(DEBUG_HARDWARE, "Tuya Monitor: device %s returned %d bytes on read (errno was %d)", m_name.c_str(), numbytes, m_tuyaclient->getlasterror());
	}
	return numbytes;
}


void TuyaMonitor::MonitorThread()
{
	unsigned long timeval = 0;
	std::string payload;

	int numbytes = 0;
	int payload_len = 0;

	Json::Value jStatus;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());

	m_tuyaclient->setAsyncMode();
	while (!IsStopRequested(1) && (m_devicedata->connectstate == device::tuya::connectstate::CONNECTED))
	{
		if (numbytes > 0)
		{
			// received data => make new request for data point updates for switch state, power and voltage
			payload = "{\"dpId\":[1,19,20]}";
			payload_len = m_tuyaclient->BuildTuyaMessage(m_cMessageBuffer, TUYA_UPDATEDPS, payload, m_key);
		}
		else
		{
			// send heart beat to keep connection alive
			payload = "{\"gwId\":\"" + m_id + "\",\"devId\":\"" + m_id + "\"}";
			payload_len = m_tuyaclient->BuildTuyaMessage(m_cMessageBuffer, TUYA_HEART_BEAT, payload, m_key);
		}

		m_tuyaclient->send(m_cMessageBuffer, payload_len);

		numbytes = ReadFromDevice(10); // 10 seconds
		if (numbytes > 0)
		{
			std::string tuyaresponse = m_tuyaclient->DecodeTuyaMessage(m_cMessageBuffer, numbytes, m_key);

			jReader->parse(tuyaresponse.c_str(), tuyaresponse.c_str() + tuyaresponse.size(), &jStatus, nullptr);
			if (jStatus.isMember("dps"))
			{
				if (jStatus["dps"].isMember("1"))
				{
					bool oldswitchstate = m_devicedata->switchstate;
					m_devicedata->switchstate = jStatus["dps"]["1"].asBool();
					if (m_waitForSwitch)
						m_waitForSwitch = false;
					else if (m_devicedata->switchstate != oldswitchstate)
					{
						_log.Log(LOG_STATUS, "Tuya Monitor: device %s was manually switched", m_name.c_str());
						sigSendSwitch(m_devicedata);
					}
				}

				if (m_isPowerMeter)
				{
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
					if (jStatus["dps"].isMember("20"))
					{
						m_devicedata->voltage = jStatus["dps"]["20"].asUInt();
						sigSendVoltage(m_devicedata);
					}
				}
			}
		}
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
	int payload_len = m_tuyaclient->BuildTuyaMessage(m_cMessageBuffer, TUYA_CONTROL, payload, m_key);

	m_waitForSwitch = true;
	int numbytes = m_tuyaclient->send(m_cMessageBuffer, payload_len);
	for (int i = 0; i < 20; i++)
	{
		// wait for monitor thread to confirm new switch state
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		if (!m_waitForSwitch)
			return true;
	}
	m_waitForSwitch = false;
	return false;
}


void TuyaMonitor::SetMeterStartData(const double usageHigh, const double usageLow)
{
	m_devicedata->usageLow = usageLow;
	m_devicedata->usageHigh = usageHigh;
}

