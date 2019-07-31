#pragma once

#include "hardware/DomoticzHardware.h"
#include "hardware/hardwaretypes.h"
#include "typedef/metertypes.hpp"
#include "jsoncpp/json.h"


namespace Json
{
	class Value;
};

class Lyric : public CDomoticzHardwareBase
{
public:
	Lyric(const int ID, const std::string &Username, const std::string &Password, const std::string &Extra);
	~Lyric(void);
	bool WriteToHardware(const char *pdata, const unsigned char length) override;
private:
	void SetSetpoint(const int idx, const float temp, const int nodeid);
	void SetPauseStatus(const int idx, bool bHeating, const int nodeid);
	void SendOnOffSensor(const int NodeID, const device::_switch::type::value switchtype, const bool SwitchState, const std::string &defaultname);
	void SendSetPointSensor(const unsigned char Idx, const float Temp, const std::string &defaultname);
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
		device::meter::temperature::unit::value temperatureUnit;
	};

	Json::Value m_locationInfo;
	std::vector<lyricDevice> m_lyricDevices;
};
