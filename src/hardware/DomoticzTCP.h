#pragma once

#if defined WIN32
#include "ws2tcpip.h"
#endif
#include "protocols/ASyncTCP.h"
#include "DomoticzHardware.h"
#include "RFXBase.h"

class DomoticzTCP : public CRFXBase, ASyncTCP
{
      public:
	DomoticzTCP(int ID, const std::string &IPAddress, unsigned short usIPPort, const std::string &username, const std::string &password);
	~DomoticzTCP() override = default;
	bool WriteToHardware(const char *pdata, unsigned char length) override;

#ifndef NOCLOUD
	void SetConnected(bool connected);
	void Authenticated(const std::string &aToken, bool authenticated);
	void FromProxy(const unsigned char *data, size_t datalen);
	bool CompareToken(const std::string &aToken);
	bool CompareId(const std::string &instanceid);
	bool ConnectInternalProxy();
	void DisconnectProxy();
	bool isConnected();
#endif

      private:
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();

#ifndef NOCLOUD
	bool StartHardwareProxy();
	void writeProxy(const char *data, size_t size);
	bool isConnectedProxy();
	bool IsMyDomoticzAPIKey(const std::string &IPAddress);
	bool StopHardwareProxy();
	std::string GetToken();
#endif

      protected:
	void OnConnect() override;
	void OnDisconnect() override;
	void OnData(const unsigned char *pData, size_t length) override;
	void OnError(const boost::system::error_code &error) override;

      public:
	boost::signals2::signal<void()>	sDisconnected;

      private:
	std::string m_szIPAddress;
	unsigned short m_usIPPort;
	std::string m_username;
	std::string m_password;
	bool m_bIsAuthenticated;
	std::shared_ptr<std::thread> m_thread;

#ifndef NOCLOUD
	std::string token;
	bool b_ProxyConnected;
	bool b_useProxy;
#endif

};
