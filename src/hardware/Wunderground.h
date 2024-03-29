#pragma once

#include "hardware/DomoticzHardware.h"

class CWunderground : public CDomoticzHardwareBase
{
      public:
	CWunderground(int ID, const std::string &APIKey, const std::string &Location);
	~CWunderground() override = default;
	bool WriteToHardware(const char *pdata, unsigned char length) override;
	std::string GetForecastURL();

      private:
	void Init();
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();
	void GetMeterDetails();
	std::string GetWeatherStationFromGeo();

      private:
	bool m_bForceSingleStation;
	bool m_bFirstTime;
	std::string m_APIKey;
	std::string m_Location;
	std::shared_ptr<std::thread> m_thread;
};
