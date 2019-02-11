/******************************************************************************
    Copyright (C) 2019  Gordon Bos

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _HARMONYHUBPP
#define _HARMONYHUBPP

#include <functional>
#include <vector>
#include <string>
#include <jsoncpp/json.h>


namespace harmonyhubpp {
namespace activity {
namespace status {
	enum value
	{
		off = 0,
		switching_on,
		on,
		switching_off
	};
}; // namespace status
}; // namespace activity

namespace session {
namespace status {
	enum value
	{
		connecting = 0,
		open,
		failed,
		closed
	};
}; // namespace status
}; // namespace session


typedef std::function<void(std::string)> message_handler;

struct hubresource
{
	std::string ID;
	std::string name;
};

#ifndef without_discovery
	/* inline subclass that does a HTTP call to the HarmonyHub to retrieve
	 * the Hub ID and domain name
	 */
	class HarmonyDiscovery;
#endif

	/* inline subclass that isolates the websocket functions to allow easier
	 * integration into larger projects
	 */
	class HarmonyConnection;





class HarmonyClient
{
private:
	std::error_code send_request(const std::string cmd, const std::string params="");

	// response handlers
	void handle_notification(const Json::Value &j_data);
	void handle_reply(const Json::Value &j_data);
	void parse_message(const std::string szdata);

public:
	HarmonyClient();
	~HarmonyClient();

	void set_hubid(const std::string remoteID, const std::string domain="");
	std::string get_hubid();
	std::string get_domain();

	std::error_code connect(const std::string IPAddress);
	std::error_code close(const std::string reason="");

	void set_message_handler(harmonyhubpp::message_handler h);


	/*
	 * Ping
	 *
	 * Harmony Hub requires that you send a message to it at least once every
	 * 40 seconds to keep the connection alive and receive notifications.
	*/
	std::error_code ping();

	/****************************************************************
	 *								*
	 * Query actions						*
	 *								*
	 *								*
	 *								*
	 ****************************************************************/
	std::error_code get_userinfo();
	std::error_code get_statedigest();
	std::error_code get_current_activity();
	std::error_code get_config();
	std::error_code get_activitylist();
	std::error_code get_devicelist();

	// miscellaneous
	std::error_code get_capabilitylist();
	std::error_code get_homeautomation_config();

	/****************************************************************
	 *								*
	 * Command actions						*
	 *								*
	 *								*
	 *								*
	 ****************************************************************/

	/*
	 * Start an activity.
	 * 
	 * Args:
	 *  activity_id (str): Activity ID from Harmony Hub configuration to control
	*/
	std::error_code start_activity(std::string activity_id);


	/*
	 * Send an action to the Harmony Hub.
	 * 
	 * Args:
	 *  action (str): Action from Harmony Hub configuration to control
	 *  delay_ms (int): Delay in milliseconds between sending the press command and the release command.
	*/
	std::error_code send_action(const std::string action, const int delay_ms=0);

	/*
	 * Send a simple command to the Harmony Hub.
	 * 
	 * Args:
	 *  device_id (str): Device ID from Harmony Hub configuration to control
	 *  cmd (str): Command from Harmony Hub configuration to control
	 *  delay_ms (int): Delay in milliseconds between sending the press command and the release command.
	*/
	std::error_code send_device_command(const std::string device_id, const std::string cmd, const int delay_ms=0);

	/*
	 * Change a channel
	 * 
	 * Args:
	 *  channel (int): Activity ID from Harmony Hub configuration to control
	*/
	std::error_code change_channel(int channel);


	// debug function
	std::error_code send_raw_message(const std::string message);


private:
	harmonyhubpp::HarmonyConnection *m_conn;

	std::string m_remoteID;
	std::string m_domain;
	uint m_msgid;
};


}; // namespace harmonyhubpp


#endif // _HARMONYHUBPP
