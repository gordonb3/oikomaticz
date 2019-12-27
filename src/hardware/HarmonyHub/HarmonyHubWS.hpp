#pragma once

#include "hardware/DomoticzHardware.h"
#include "hardware/hardwaretypes.h"
#include "main/json_helper.h"
#include "harmonyhubclient.hpp"

namespace harmonyhubpp {
namespace connection {
namespace status {
	enum value {
		connecting,
		connected,
		closing,
		closed
	};
}; // namespace status
}; // namespace connection
}; // namespace harmonyhub

namespace hardware {
namespace handler {

class HarmonyHubWS : public CDomoticzHardwareBase
{
public:
	HarmonyHubWS(const int ID, const std::string &IPAddress);
	~HarmonyHubWS(void);
	bool WriteToHardware(const char *pdata, const unsigned char length) override;
private:
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();

	// Init and cleanup
	void Init();

	// Helper functions for changing switch status in Domoticz
	void CheckSetActivity(const std::string &activityID, const bool on);
	void UpdateSwitch(const unsigned char idx, const char * szIdx, const bool bOn, const std::string &defaultname);


	void AsyncReceiver(const std::string szdata);
	void ProcessHarmonyNotification(const Json::Value &j_data);
	void ProcessHarmonyResponse(const Json::Value &j_data);


	void ConnectToHub();
	void Disconnect();


	int SendPing();


private:
	// hardware parameters
	std::string m_szHarmonyAddress;
	unsigned short m_usHarmonyPort;

	std::shared_ptr<std::thread> m_thread;
	std::mutex m_mutex;

	harmonyhubpp::HarmonyClient m_HarmonyClient;
	harmonyhubpp::connection::status::value m_connectionstatus;

	bool m_bLoginNow;
	bool m_bShowConnectError;
	bool m_bReceivedNotification;
	bool m_bIsChangingActivity;
	bool m_bWantAnswer;
	bool m_bRequireEcho;

	std::string m_szHubSwVersion;
	std::string m_szCurActivityID;
	std::map<std::string, std::string> m_mapActivities;
};

}; //namespace handler
}; //namespace hardware
