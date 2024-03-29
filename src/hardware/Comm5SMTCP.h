#pragma once

#include <deque>
#include <iostream>
#include "protocols/ASyncTCP.h"
#include "hardware/DomoticzHardware.h"

class Comm5SMTCP : public CDomoticzHardwareBase, ASyncTCP
{
      public:
	Comm5SMTCP(int ID, const std::string &IPAddress, unsigned short usIPPort);
	bool WriteToHardware(const char *pdata, unsigned char length) override;
	boost::signals2::signal<void()> sDisconnected;

      private:
	bool StartHardware() override;
	bool StopHardware() override;

      protected:
	void OnConnect() override;
	void OnDisconnect() override;
	void OnData(const unsigned char *pData, size_t length) override;
	void OnError(const boost::system::error_code &error) override;

	void Do_Work();
	void ParseData(const unsigned char *data, size_t len);
	void querySensorState();

      private:
	std::string m_szIPAddress;
	unsigned short m_usIPPort;
	std::string buffer;
	bool initSensorData;
	bool m_bReceiverStarted;
	std::shared_ptr<std::thread> m_thread;
};
