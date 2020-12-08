#pragma once

#include "hardware/DomoticzHardware.h"
#include "hardware/hardwaretypes.h"

namespace Json
{
	class Value;
};

class EnphaseAPI : public CDomoticzHardwareBase
{
      public:
	EnphaseAPI(int ID, const std::string &IPAddress, unsigned short usIPPort);
	~EnphaseAPI() override = default;
	bool WriteToHardware(const char *pdata, unsigned char length) override;

      private:
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();

	bool getProductionDetails(Json::Value &result);

	void parseProduction(const Json::Value &root);
	void parseConsumption(const Json::Value &root);
	void parseNetConsumption(const Json::Value &root);

	int getSunRiseSunSetMinutes(bool bGetSunRise);

      private:
	std::string m_szIPAddress;
	P1Power m_p1power;
	P1Power m_c1power;
	P1Power m_c2power;
	std::shared_ptr<std::thread> m_thread;
};
