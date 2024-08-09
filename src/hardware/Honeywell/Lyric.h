#pragma once

#include "hardware/DomoticzHardware.h"
#include "hardware/hardwaretypes.h"
#include "typedef/metertypes.hpp"
#include "main/json_helper.h"


namespace Json
{
	class Value;
} // namespace Json

class Lyric : public CDomoticzHardwareBase
{
      public:
	Lyric(int ID, const std::string &Username, const std::string &Password, const std::string &Extra);
	~Lyric() override = default;;
	bool WriteToHardware(const char *pdata, unsigned char length) override;
      private:
	void SetSetpoint(int idx, float temp, int nodeid);
	void SetPauseStatus(int idx, bool bHeating, int nodeid);
	void SendOnOffSensor(int NodeID, device::tswitch::type::value switchtype, bool SwitchState, const std::string &defaultname);
	bool refreshToken();
	void Init();
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();
	void GetThermostatData();
      private:
	std::string mApiKey;
	std::string mApiSecret;
	std::string mAccessToken;
	std::string mRefreshToken;
	time_t mTokenExpires = { 0 };
	std::string mThermostatID;
	int mOutsideTemperatureIdx;
	bool mIsStarted;
	std::shared_ptr<std::thread> m_thread;
	std::vector<std::string> mSessionHeaders;
	int mLastMinute;

	struct lyricDevice
	{
		Json::Value *deviceInfo;
		int devNr;
		std::string locationId;
		std::string deviceName;
		device::tmeter::temperature::unit::value temperatureUnit;
	};

	Json::Value m_locationInfo;
	std::vector<lyricDevice> m_lyricDevices;
};
