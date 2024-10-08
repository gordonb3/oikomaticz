#pragma once

#include "hardware/DomoticzHardware.h"
#include "hardware/hardwaretypes.h"

class CYouLess : public CDomoticzHardwareBase
{
      public:
	struct YouLessMeter
	{
		unsigned char len;
		unsigned char type;
		unsigned char subtype;
		unsigned short ID1;
		unsigned long powerusage;
		unsigned long usagecurrent;
	};
	CYouLess(int ID, const std::string &IPAddress, unsigned short usIPPort, const std::string &password);
	~CYouLess() override = default;
	bool WriteToHardware(const char *pdata, unsigned char length) override;

      private:
	void Init();
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();
	void GetMeterDetails();
	bool GetP1Details();

      private:
	std::string m_szIPAddress;
	unsigned short m_usIPPort;
	std::string m_Password;
	std::shared_ptr<std::thread> m_thread;

	YouLessMeter m_meter;
	bool m_bCheckP1;
	bool m_bHaveP1OrS0;
	P1Power	m_p1power;
	P1BusDevice	m_p1gas;
	unsigned long m_lastgasusage;
	time_t m_lastSharedSendGas;
};
