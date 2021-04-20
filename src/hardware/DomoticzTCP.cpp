#include "stdafx.h"
#include "DomoticzTCP.h"
#include "main/Logger.h"
#include "main/Helper.h"
#include "main/localtime_r.h"
#include "main/mainworker.h"
#include "main/WebServerHelper.h"

#ifndef NOCLOUD
#include "webserver/proxyclient.h"

extern http::server::CWebServerHelper m_webservers;
#endif


#define RETRY_DELAY_SECONDS 30
#define SLEEP_MILLISECONDS 100
#define HEARTBEAT_SECONDS 12

#define MY_DOMOTICZ_APIKEY_NUMCHARS 15



DomoticzTCP::DomoticzTCP(const int ID, const std::string &IPAddress, const unsigned short usIPPort, const std::string &username, const std::string &password)
	: m_szIPAddress(IPAddress)
	, m_username(username)
	, m_password(password)
	, m_usIPPort(usIPPort)
{
	m_HwdID = ID;
	m_bIsStarted = false;
#ifndef NOCLOUD
	b_useProxy = IsMyDomoticzAPIKey(m_szIPAddress);
	b_ProxyConnected = false;
#endif
}


bool DomoticzTCP::StartHardware()
{
	RequestStart();

	if (m_username.empty())
	{
		_log.Log(LOG_ERROR, "DomoticzTCP: cannot authenticate with remote due to missing username");
		return false;
	}

#ifndef NOCLOUD
	b_useProxy = IsMyDomoticzAPIKey(m_szIPAddress);
	if (b_useProxy) {
		// don't create worker thread
		return StartHardwareProxy();
	}
#endif

	//Start worker thread
	m_thread = std::make_shared<std::thread>([this] { Do_Work(); });
	SetThreadNameInt(m_thread->native_handle());

	return (m_thread != nullptr);
}

bool DomoticzTCP::StopHardware()
{
#ifndef NOCLOUD
	if (b_useProxy) {
		return StopHardwareProxy();
	}
#endif
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	if (ASyncTCP::isConnected())
		disconnect();
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
				_log.Log(LOG_STATUS, "DomoticzTCP: attempt connect to %s:%d", m_szIPAddress.c_str(), m_usIPPort);
				connect(m_szIPAddress, m_usIPPort);
			}
			retry_counter++;
		}

		heartbeat_counter++;
		if ((heartbeat_counter % (HEARTBEAT_SECONDS * 1000 / SLEEP_MILLISECONDS)) == 0)
			mytime(&m_LastHeartbeat);
	}
	terminate();

	_log.Log(LOG_STATUS, "DomoticzTCP: TCP/IP Worker stopped...");
}


bool DomoticzTCP::WriteToHardware(const char *pdata, const unsigned char length)
{
#ifndef NOCLOUD
	if (b_useProxy)
	{
		if (isConnectedProxy())
		{
			writeProxy(pdata, length);
			return true;
		}
		return false;
	}
#endif
	if (ASyncTCP::isConnected())
	{
		write((const unsigned char*)pdata, length);
		return true;
	}
	return false;
}


#ifndef NOCLOUD
bool DomoticzTCP::isConnected()
{
	if (b_useProxy) {
		return isConnectedProxy();
	}
	return ASyncTCP::isConnected();
}
#endif


/****************************************************************
/								/
/	Async TCP entry points					/
/								/
/***************************************************************/

void DomoticzTCP::OnConnect()
{
	_log.Log(LOG_STATUS, "DomoticzTCP: connected to: %s:%d", m_szIPAddress.c_str(), m_usIPPort);

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
		_log.Log(LOG_ERROR, "DomoticzTCP: Can not connect to: %s:%d (%s)", m_szIPAddress.c_str(), m_usIPPort, error.message().c_str());
	}
	else if (
		(error == boost::asio::error::eof) ||
		(error == boost::asio::error::connection_reset)
		)
	{
		if (!m_bIsAuthenticated)
		{
			_log.Log(LOG_ERROR, "DomoticzTCP: Authentication with remote failed! Wrong password?");
			StopHardware();
		}
		_log.Log(LOG_STATUS, "DomoticzTCP: Connection reset!");
	}
	else
	{
		_log.Log(LOG_ERROR, "DomoticzTCP: %s", error.message().c_str());
	}
}


/****************************************************************
/								/
/	My Domoticz HTTP connection functions			/
/								/
/***************************************************************/

#ifndef NOCLOUD
bool DomoticzTCP::IsMyDomoticzAPIKey(const std::string &IPAddress)
{
	if (IPAddress.find_first_of(".:") != std::string::npos) {
		// input appears to be an IP address or DNS name
		return false;
	}
	// just a simple check
	return IPAddress.length() == MY_DOMOTICZ_APIKEY_NUMCHARS;
}


bool DomoticzTCP::StartHardwareProxy()
{
	if (m_bIsStarted) {
		return false; // dont start twice
	}
	m_bIsStarted = true;
	return ConnectInternalProxy();
}


bool DomoticzTCP::ConnectInternalProxy()
{
	http::server::CProxyClient *proxy;
	const int version = 1;
	// we temporarily use the instance id as an identifier for this connection, meanwhile we get a token from the proxy
	// this means that we connect twice to the same server
	token = m_szIPAddress;
	proxy = m_webservers.GetProxyForMaster(this);
	if (proxy) {
		proxy->ConnectToDomoticz(m_szIPAddress, m_username, m_password, this, version);
		sOnConnected(this); // we do need this?
	}
	else {
		_log.Log(LOG_STATUS, "Delaying Domoticz master login");
	}
	return true;
}


bool DomoticzTCP::StopHardwareProxy()
{
	DisconnectProxy();
	m_bIsStarted = false;
	// Avoid dangling pointer if this hardware is removed.
	m_webservers.RemoveMaster(this);
	return true;
}


void DomoticzTCP::DisconnectProxy()
{
	http::server::CProxyClient *proxy;

	proxy = m_webservers.GetProxyForMaster(this);
	if (proxy) {
		proxy->DisconnectFromDomoticz(token, this);
	}
	b_ProxyConnected = false;
}


bool DomoticzTCP::isConnectedProxy()
{
	return b_ProxyConnected;
}


void DomoticzTCP::writeProxy(const char *data, size_t size)
{
	/* send data to My Domoticz */
	if (isConnectedProxy()) {
		http::server::CProxyClient *proxy = m_webservers.GetProxyForMaster(this);
		if (proxy) {
			proxy->WriteMasterData(token, data, size);
		}
	}
}


/****************************************************************
/								/
/	functions called by webserver/proxyclient		/
/								/
/***************************************************************/

void DomoticzTCP::FromProxy(const unsigned char *data, size_t datalen)
{
	/* data received from My Domoticz */
	std::lock_guard<std::mutex> l(readQueueMutex);
	onInternalMessage(data, datalen);
}


bool DomoticzTCP::CompareToken(const std::string &aToken)
{
	return (aToken == token);
}


bool DomoticzTCP::CompareId(const std::string &instanceid)
{
	return (m_szIPAddress == instanceid);
}


std::string DomoticzTCP::GetToken()
{
	return token;
}


void DomoticzTCP::Authenticated(const std::string &aToken, bool authenticated)
{
	b_ProxyConnected = authenticated;
	token = aToken;
	if (authenticated) {
		_log.Log(LOG_STATUS, "Domoticz TCP connected via Proxy.");
	}
}


void DomoticzTCP::SetConnected(bool connected)
{
	if (connected) {
		ConnectInternalProxy();
	}
	else {
		b_ProxyConnected = false;
	}
}
#endif
