#include "stdafx.h"
#include "DomoticzTCP.h"
#include "main/Logger.h"
#include "main/Helper.h"
#include "main/mainworker.h"
#include "main/WebServerHelper.h"

#define RETRY_DELAY_SECONDS 30
#define SLEEP_MILLISECONDS 100
#define HEARTBEAT_SECONDS 12

extern http::server::CWebServerHelper m_webservers;

DomoticzTCP::DomoticzTCP(const int ID, const std::string &IPAddress, const unsigned short usIPPort, const std::string &username, const std::string &password)
	: m_szIPAddress(IPAddress)
	, m_username(username)
	, m_password(password)
	, m_usIPPort(usIPPort)
{
	m_HwdID = ID;
	m_bIsStarted = false;
}


bool DomoticzTCP::StartHardware()
{
	RequestStart();

	//Start worker thread
	m_thread = std::make_shared<std::thread>([this] { Do_Work(); });
	SetThreadNameInt(m_thread->native_handle());

	return (m_thread != nullptr);
}

bool DomoticzTCP::StopHardware()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	if (ASyncTCP::isConnected())
		disconnect();
	m_bIsStarted = false;
	return true;
}


void DomoticzTCP::Do_Work()
{
	int heartbeat_counter = 0;
	int retry_counter = 0;
	while (!IsStopRequested(100))
	{
		if (!ASyncTCP::isConnected())
		{
			if ((retry_counter % (RETRY_DELAY_SECONDS * 1000 / SLEEP_MILLISECONDS)) == 0)
			{
				_log.Log(LOG_STATUS, "attempt connect to %s:%d", m_szIPAddress.c_str(), m_usIPPort);
				connect(m_szIPAddress, m_usIPPort);
			}
			retry_counter++;
		}

		heartbeat_counter++;
		if ((heartbeat_counter % (HEARTBEAT_SECONDS * 1000 / SLEEP_MILLISECONDS)) == 0)
			mytime(&m_LastHeartbeat);
	}
	terminate();

	_log.Log(LOG_STATUS, "TCP/IP Worker stopped...");
}


bool DomoticzTCP::WriteToHardware(const char *pdata, const unsigned char length)
{
	if (ASyncTCP::isConnected())
	{
		write((const unsigned char*)pdata, length);
		return true;
	}
	return false;
}


/****************************************************************
/								/
/	Async TCP entry points					/
/								/
/***************************************************************/

void DomoticzTCP::OnConnect()
{
	_log.Log(LOG_STATUS, "connected to: %s:%d", m_szIPAddress.c_str(), m_usIPPort);

	if (!m_username.empty())
	{
		m_bIsAuthenticated = false;
		std::string sAuth = std_format("AUTH;%s;%s", m_username.c_str(), m_password.c_str());
		WriteToHardware(sAuth.c_str(), (unsigned char)sAuth.size());
	}
}


void DomoticzTCP::OnDisconnect()
{
	_log.Log(LOG_STATUS, "disconnected from: %s:%d", m_szIPAddress.c_str(), m_usIPPort);
}


void DomoticzTCP::OnData(const unsigned char *pData, size_t length)
{
	if (length == 6 && strstr(reinterpret_cast<const char *>(pData), "NOAUTH") != nullptr)
	{
		Log(LOG_ERROR, "Authentication failed for user %s on %s:%d", m_username.c_str(), m_szIPAddress.c_str(), m_usIPPort);
		return;
	}

	std::lock_guard<std::mutex> l(readQueueMutex);
	onInternalMessage((const unsigned char *)pData, length, false); // Do not check validity, this might be non RFX-message
}


void DomoticzTCP::OnError(const boost::system::error_code& error)
{
	if (
		(error == boost::asio::error::address_in_use) ||
		(error == boost::asio::error::connection_refused) ||
		(error == boost::asio::error::access_denied) ||
		(error == boost::asio::error::host_unreachable) ||
		(error == boost::asio::error::timed_out) ||
		(error == boost::asio::error::host_not_found)
		)
	{
		_log.Log(LOG_ERROR, "Can not connect to: %s:%d (%s)", m_szIPAddress.c_str(), m_usIPPort, error.message().c_str());
	}
	else if (
		(error == boost::asio::error::eof) ||
		(error == boost::asio::error::connection_reset)
		)
	{
		if (!m_bIsAuthenticated)
		{
			_log.Log(LOG_ERROR, "Authentication with remote failed! Wrong password?");
			StopHardware();
		}
		_log.Log(LOG_STATUS, "Connection reset!");
	}
	else
	{
		_log.Log(LOG_ERROR, " %s", error.message().c_str());
	}
}



