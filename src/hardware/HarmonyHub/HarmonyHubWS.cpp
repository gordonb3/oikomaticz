/******************************************************************************
    Copyright (C) 2019  gordonb3

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

// #define _DEBUG


#include "stdafx.h"
#include "HarmonyHubWS.hpp"
#include "main/Helper.h"
#include "main/Logger.h"
#include "main/localtime_r.h"
#include "main/RFXtrx.h"
#include "main/SQLHelper.h"


#define HARMONY_PING_INTERVAL_SECONDS		35	// must be smaller than 40 seconds or Hub will silently hang up on us
#define HARMONY_SEND_ACK_SECONDS		15	// must be smaller than 20 seconds...
#define HARMONY_PING_RETRY_SECONDS		 5
#define HARMONY_RETRY_LOGIN_SECONDS		60
#define HEARTBEAT_SECONDS			12


namespace hardware {
namespace handler {

#ifdef _DEBUG
#include <iostream>

#ifndef _WIN32
#define sprintf_s(buffer, buffer_size, stringbuffer, ...) (sprintf(buffer, stringbuffer, __VA_ARGS__))
#endif

std::string datetime()
{
	time_t now=time(0);
	struct tm ltime;
	localtime_r(&now, &ltime);

	char szTime[20];
	sprintf_s(szTime, 19, "%04d-%02d-%02d %02d:%02d:%02d", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	return std::string(szTime, 19);
}

#endif


HarmonyHubWS::HarmonyHubWS(const int ID, const std::string &IPAddress):
m_szHarmonyAddress(IPAddress)
{
	m_HwdID = ID;
	m_HarmonyClient.set_message_handler(boost::bind(&HarmonyHubWS::AsyncReceiver, this, boost::placeholders::_1));
	Init();
}


HarmonyHubWS::~HarmonyHubWS()
{
	StopHardware();
}


void HarmonyHubWS::Init()
{
	m_connectionstatus = harmonyhubpp::connection::status::closed;
	m_bWantAnswer = false;
	m_bRequireEcho = false;
	m_bReceivedNotification = false;
	m_bIsChangingActivity = false;
	m_bShowConnectError = true;
	m_szCurActivityID = "";
	m_szHubSwVersion = "";
	m_mapActivities.clear();
}


bool HarmonyHubWS::StartHardware()
{
	RequestStart();

	Init();
	//Start worker thread
	m_thread = std::make_shared<std::thread>([this] { Do_Work(); });
	SetThreadNameInt(m_thread->native_handle());
	m_bIsStarted = true;
	sOnConnected(this);
	return (m_thread != nullptr);
}


bool HarmonyHubWS::StopHardware()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	m_bIsStarted = false;
	return true;
}


bool HarmonyHubWS::WriteToHardware(const char *pdata, const unsigned char /*length*/)
{
	const tRBUF *pCmd = reinterpret_cast<const tRBUF*>(pdata);

	if (this->m_bIsChangingActivity)
	{
		_log.Log(LOG_ERROR, "Harmony Hub: Command cannot be sent. Hub is changing activity");
		return false;
	}

	if (this->m_connectionstatus == harmonyhubpp::connection::status::closed)
	{
		if (
		     (pCmd->LIGHTING2.id1 == 0xFF) && (pCmd->LIGHTING2.id2 == 0xFF) &&
		     (pCmd->LIGHTING2.id3 == 0xFF) && (pCmd->LIGHTING2.id4 == 0xFF) &&
		     (pCmd->LIGHTING2.cmnd == 0)
		)
		{
			// "secret" undefined state request to silence connection error reporting
			if (this->m_bShowConnectError)
				_log.Log(LOG_STATUS, "Harmony Hub: disable connection error logging");
			this->m_bShowConnectError = false;
			return false;
		}

		_log.Log(LOG_STATUS, "Harmony Hub: Received a switch command but we are not connected - attempting connect now");
		this->m_bLoginNow = true;
		int retrycount = 0;
		while ( (retrycount < 10) && (!IsStopRequested(500)) )
		{
			// give worker thread up to 5 seconds time to establish the connection
			if ((this->m_connectionstatus == harmonyhubpp::connection::status::connected) && (!m_szCurActivityID.empty()))
				break;
			retrycount++;
		}
		if (IsStopRequested(0))
			return true;

		if (this->m_connectionstatus == harmonyhubpp::connection::status::closed)
		{
			_log.Log(LOG_ERROR, "Harmony Hub: Connect failed: cannot send the switch command");
			return false;
		}
	}

	if (pCmd->LIGHTING2.packettype == pTypeLighting2)
	{
		int lookUpId = (int)(pCmd->LIGHTING2.id1 << 24) |  (int)(pCmd->LIGHTING2.id2 << 16) | (int)(pCmd->LIGHTING2.id3 << 8) | (int)(pCmd->LIGHTING2.id4) ;
		std::string realID = std::to_string(lookUpId);

		m_bWantAnswer = true;
		if (pCmd->LIGHTING2.cmnd == 0)
		{
			if (m_szCurActivityID != realID)
			{
				// don't send power off if the ID does not match the current activity
				m_bWantAnswer = false;
				return false;
			}
			if (realID == "-1")
			{
				// powering off the PowerOff activity leads to an undefined state in the frontend
				// send it anyway: user may be trying to correct a wrong state of the Hub
				m_HarmonyClient.start_activity("-1");
				// but don't allow the frontend to update the button state to the off position
				return false;
			}
			if (m_HarmonyClient.start_activity("-1"))
			{
				_log.Log(LOG_ERROR, "Harmony Hub: Error sending the power-off command");
				return false;
			}
		}
		else if (m_HarmonyClient.start_activity(realID))
		{
			_log.Log(LOG_ERROR, "Harmony Hub: Error sending the switch command");
			return false;
		}
	}
	return true;
}


void HarmonyHubWS::Do_Work()
{
	_log.Log(LOG_STATUS,"Harmony Hub: Worker thread started...");

	unsigned int pcounter = 0;		// ping interval counter
	unsigned int tcounter = 0;		// 1/25 seconds

	char lcounter = 0;			// login counter
	char fcounter = 0;			// failed login attempts
	char hcounter = HEARTBEAT_SECONDS;	// heartbeat interval counter


	m_bLoginNow = true;

	while (!IsStopRequested(0))
	{
		if (m_bReceivedNotification)
		{
			if ((pcounter % HARMONY_PING_INTERVAL_SECONDS) < (HARMONY_PING_INTERVAL_SECONDS - HARMONY_SEND_ACK_SECONDS))
			{
				// fast forward our ping counter
				pcounter = HARMONY_PING_INTERVAL_SECONDS - HARMONY_SEND_ACK_SECONDS;
#ifdef _DEBUG
				std::cerr << "fast forward ping counter to " << pcounter << " seconds\n";
#endif
			}
			m_bReceivedNotification = false;
		}

		if (!m_bWantAnswer && m_connectionstatus == harmonyhubpp::connection::status::connected)
		{
			if (m_mapActivities.empty())
			{
				// instruct Hub to send us its config so we can retrieve the list of activities
				m_bWantAnswer = true;
				m_HarmonyClient.get_config();
			}
			else if (m_szCurActivityID.empty())
			{
				fcounter = 0;
				m_bWantAnswer = true;
				m_HarmonyClient.get_current_activity();
			}
		}

		if (!m_bWantAnswer && !tcounter) // slot this to full seconds only to prevent racing
		{
			if (m_connectionstatus == harmonyhubpp::connection::status::connected)
			{
				// Hub requires us to actively keep our connection alive by periodically pinging it
				if ((pcounter % HARMONY_PING_INTERVAL_SECONDS) == 0)
				{
#ifdef _DEBUG
					std::cerr << "send ping\n";
#endif
					if (m_bRequireEcho || SendPing() != 0)
					{
						// Hub dropped our connection
						_log.Log(LOG_ERROR, "Harmony Hub: Error pinging server.. Resetting connection.");
						Disconnect();
						pcounter = HARMONY_RETRY_LOGIN_SECONDS - 5; // wait 5 seconds before attempting login again
					}
				}

				else if (m_bRequireEcho && ((pcounter % HARMONY_PING_RETRY_SECONDS) == 0))
				{
					// timeout
#ifdef _DEBUG
					std::cerr << "retry ping\n";
#endif
					if (SendPing() != 0)
					{
						// Hub dropped our connection
						_log.Log(LOG_ERROR, "Harmony Hub: Error pinging server.. Resetting connection.");
						Disconnect();
						pcounter = HARMONY_RETRY_LOGIN_SECONDS - 5; // wait 5 seconds before attempting login again
					}
				}
			}

			else if (m_connectionstatus == harmonyhubpp::connection::status::closed)
			{
				if (!m_bLoginNow)
				{
					if ((pcounter % HARMONY_RETRY_LOGIN_SECONDS) == 0)
					{
						lcounter++;
						if (lcounter > fcounter)
						{
							m_bLoginNow = true;
							if (fcounter > 0)
								_log.Log(LOG_NORM, "Harmony Hub: Reattempt login.");
						}
					}
				}
				if (m_bLoginNow)
				{
					m_bLoginNow = false;
					m_bWantAnswer = false;
					m_bRequireEcho = false;
					if (fcounter < 5)
						fcounter++;
					lcounter = 0;
					pcounter = 0;
					m_szCurActivityID = "";

					ConnectToHub();
				}
			}

			else
			{
				// m_connectionstatus == connecting || closing
				if ((pcounter % HARMONY_RETRY_LOGIN_SECONDS) > 1)
				{
					// timeout
					_log.Log(LOG_ERROR, "Harmony Hub: setup command socket timed out");
					m_connectionstatus = harmonyhubpp::connection::status::closed;
//					ResetCommunicationSocket();
				}
			}
		}

		if (m_bWantAnswer || tcounter)
		{
			// use a 40ms poll interval
			sleep_milliseconds(40);
			tcounter++;
			if ((tcounter % 25) == 0)
			{
				tcounter = 0;
				pcounter++;
				hcounter--;
			}
		}
		else
		{
			// use a 1s poll interval
			if (IsStopRequested(1000))
				break;
			pcounter++;
			hcounter--;
		}

		if (!hcounter)
		{
			// update heartbeat
			hcounter = HEARTBEAT_SECONDS;
			m_LastHeartbeat = mytime(nullptr);
		}
	}
	Disconnect();

	_log.Log(LOG_STATUS,"Harmony Hub: Worker stopped...");
}


/************************************************************************
*									*
* Update a single activity switch if exist				*
*									*
************************************************************************/
void HarmonyHubWS::CheckSetActivity(const std::string &activityID, const bool on)
{
	// get the device id from the db (if already inserted)
	int actId=atoi(activityID.c_str());
	std::stringstream hexId ;
	hexId << std::setw(7)  << std::hex << std::setfill('0') << std::uppercase << (int)( actId) ;
	std::string actHex = hexId.str();
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT Name,DeviceID FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, actHex.c_str());
	if (!result.empty())
	{
		UpdateSwitch((uint8_t)(atoi(result[0][1].c_str())), activityID.c_str(),on,result[0][0]);
	}
}


/************************************************************************
*									*
* Insert/Update a single activity switch (unconditional)		*
*									*
************************************************************************/
void HarmonyHubWS::UpdateSwitch(unsigned char /*idx*/, const char *realID, const bool bOn, const std::string &defaultname)
{
	std::stringstream hexId ;
	hexId << std::setw(7) << std::setfill('0') << std::hex << std::uppercase << (int)( atoi(realID) );
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT Name,nValue,sValue FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q')", m_HwdID, hexId.str().c_str());
	if (!result.empty())
	{
		//check if we have a change, if not do not update it
		int nvalue = atoi(result[0][1].c_str());
		if ((!bOn) && (nvalue == light2_sOff))
			return;
		if ((bOn && (nvalue != light2_sOff)))
			return;
	}
	int i_Id = atoi( realID);
	//Send as Lighting 2
	tRBUF lcmd;
	memset(&lcmd, 0, sizeof(RBUF));
	lcmd.LIGHTING2.packetlength = sizeof(lcmd.LIGHTING2) - 1;
	lcmd.LIGHTING2.packettype = pTypeLighting2;
	lcmd.LIGHTING2.subtype = sTypeAC;

	lcmd.LIGHTING2.id1 = (i_Id>> 24) & 0xFF;
	lcmd.LIGHTING2.id2 = (i_Id>> 16) & 0xFF;
	lcmd.LIGHTING2.id3 = (i_Id>> 8) & 0xFF;
	lcmd.LIGHTING2.id4 = (i_Id) & 0xFF;
	lcmd.LIGHTING2.unitcode = 1;
	uint8_t level = 15;
	if (!bOn)
	{
		level = 0;
		lcmd.LIGHTING2.cmnd = light2_sOff;
	}
	else
	{
		level = 15;
		lcmd.LIGHTING2.cmnd = light2_sOn;
	}
	lcmd.LIGHTING2.level = level;
	lcmd.LIGHTING2.filler = 0;
	lcmd.LIGHTING2.rssi = 12;
	sDecodeRXMessage(this, (const unsigned char *)&lcmd.LIGHTING2, defaultname.c_str(), 255, m_Name.c_str());
}



void HarmonyHubWS::ConnectToHub()
{
	_log.Debug(DEBUG_HARDWARE, "Harmony Hub: Attempting connect to %s", m_szHarmonyAddress.c_str());
	m_bWantAnswer = true;
	m_connectionstatus = harmonyhubpp::connection::status::connecting;
	m_HarmonyClient.connect(m_szHarmonyAddress);
}



void HarmonyHubWS::Disconnect()
{
	if (m_connectionstatus != harmonyhubpp::connection::status::closed)
	{
		_log.Debug(DEBUG_HARDWARE, "Harmony Hub: Closing connection");
		m_connectionstatus = harmonyhubpp::connection::status::closing;
		m_HarmonyClient.close();

		// wait for confirmation
		int i = 50;
		while ((i > 0) && (m_connectionstatus != harmonyhubpp::connection::status::closed))
		{
			sleep_milliseconds(40);
			i--;
		}
		if (m_connectionstatus != harmonyhubpp::connection::status::closed)
		{
			// something went wrong
			_log.Log(LOG_ERROR, "Harmony Hub: Timeout while closing connection");
			m_connectionstatus = harmonyhubpp::connection::status::closed;
		}
	}
	Init();
}


/************************************************************************
*									*
* Ping function								*
*									*
************************************************************************/
int HarmonyHubWS::SendPing()
{
	m_bRequireEcho = true;

	int retval = m_HarmonyClient.ping().value();
	if (retval > 0)
		_log.Debug(DEBUG_HARDWARE, "Harmony Hub: Ping returned error %d", retval);
	return retval;
}


/************************************************************************
*									*
* Query response handler						*
*  - handles direct responses to queries we sent to Harmony Hub		*
*									*
************************************************************************/
void HarmonyHubWS::ProcessHarmonyResponse(const Json::Value &j_data)
{
	int returncode = atoi(j_data["code"].asString().c_str());

	if (returncode == 100)
	{
		// This errorcode has been seen with `mime='harmony.engine?startActivity'`
		// We're currently not interested in those - see related comment section in ProcessHarmonyResponse()
#ifdef _DEBUG
		std::cerr << "message status = continue\n";
#endif
		return;
	}
	if (returncode != 200)
	{
		if (m_connectionstatus == harmonyhubpp::connection::status::connecting)
		{
			m_connectionstatus = harmonyhubpp::connection::status::closed;
			if (m_bShowConnectError)
				_log.Log(LOG_ERROR, "Harmony Hub: Attempt to connect to Hub returned error %d", returncode);
		}
		else
			_log.Debug(DEBUG_HARDWARE, "Harmony Hub: Error %d returned in response to %s", returncode, j_data["cmd"].asString().c_str());
		return;
	}

	std::string cmd, shortcmd;
	if (j_data.isMember("cmd"))
		cmd = j_data["cmd"].asString();
	size_t loc = cmd.find_last_of('.');
	if (loc != std::string::npos)
		shortcmd = cmd.substr(loc+1);
	else
		shortcmd = cmd;

	if (!j_data.isMember("data"))
	{
#ifdef _DEBUG
		// incomplete json data
		std::cerr << datetime() << "incomplete answer from " << cmd << "\n";
#endif
		return;
	}

	if (shortcmd == "ping")
	{
#ifdef _DEBUG
		std::cerr << "received ping return (id = " << j_data["id"].asString() << ")\n";
#endif
		m_bRequireEcho = false;
		return;
	}

	if (shortcmd.substr(0,7) != "engine?")
	{
#ifdef _DEBUG
		// unhandled
		std::cout << datetime() << " unhandled message:\n";
		std::cout << datetime() << " " << j_data.toStyledString() << std::endl;
#endif
		return;
	}

	// mime='vnd.logitech.harmony/vnd.logitech.harmony.engine?getCurrentActivity'
	if (shortcmd.substr(7) == "getCurrentActivity")
	{
		if (!j_data["data"].isMember("result"))
		{
#ifdef _DEBUG
			// incomplete json data
			std::cerr << datetime() << "incorrect answer from vnd.logitech.harmony.engine?getCurrentActivity\n";
#endif
			return;
		}

		std::string szCurrentActivity = j_data["data"]["result"].asString();
		_log.Debug(DEBUG_HARDWARE, "Harmony Hub: Current activity ID = %s (%s)", szCurrentActivity.c_str(), m_mapActivities[szCurrentActivity].c_str());
		if (m_szCurActivityID.empty()) // initialize all switches
		{
			m_szCurActivityID = szCurrentActivity;
			for (const auto & itt : m_mapActivities)
			{
				UpdateSwitch(0, itt.first.c_str(), (m_szCurActivityID == itt.first), itt.second);
			}
		}
		else if (m_szCurActivityID != szCurrentActivity)
		{
			CheckSetActivity(m_szCurActivityID, false);
			m_szCurActivityID = szCurrentActivity;
			CheckSetActivity(m_szCurActivityID, true);
		}
	}


#ifdef _DEBUG
	// mime='vnd.logitech.harmony/vnd.logitech.harmony.engine?startactivity'
	else if (shortcmd.substr(7) == "startactivity")
	{
		// doesn't appear to hold any sensible data - also always returns errorcode='200' even if activity is incorrect.
	}

	// mime='harmony.engine?startActivity'
	else if (shortcmd.substr(7) == "startActivity")
	{
		// This chain of query type messages follow after startactivity, apparently to inform that commands are being
		// executed towards specific deviceId's, but not showing what these commands are.

		// Since the mime is different from what we sent to Harmony Hub this appears to be a query directed to us, but
		// it is unknown whether we should acknowledge and/or return some response. The Hub doesn't seem to mind that
		// we don't though.
	}
#endif


	// mime='vnd.logitech.harmony/vnd.logitech.harmony.engine?config'
	else if (shortcmd.substr(7) == "config")
	{
		if (j_data["data"]["activity"].empty())
		{
			_log.Log(LOG_ERROR, "Harmony Hub: Invalid data received! (Update Activities)");
			return;
		}

		try
		{
			int totActivities = (int)j_data["data"]["activity"].size();
			for (int ii = 0; ii < totActivities; ii++)
			{
				std::string aID = j_data["data"]["activity"][ii]["id"].asString();
				std::string aLabel = j_data["data"]["activity"][ii]["label"].asString();
				m_mapActivities[aID] = aLabel;
			}
		}
		catch (...)
		{
			_log.Log(LOG_ERROR, "Harmony Hub: Invalid data received! (Update Activities, JSon activity)");
		}

		if (_log.IsDebugLevelEnabled(DEBUG_HARDWARE))
		{
			std::string resultString = "Harmony Hub: Activity list: {";

			for (auto &&activity : m_mapActivities)
			{
				resultString.append("\"");
				resultString.append(activity.second);
				resultString.append("\":\"");
				resultString.append(activity.first);
				resultString.append("\",");
			}
			resultString=resultString.substr(0, resultString.size()-1);
			resultString.append("}");

			_log.Debug(DEBUG_HARDWARE, resultString);
		}
	}

#ifdef _DEBUG
	else
	{
		// unhandled
		std::cout << datetime() << " unhandled message:\n";
		std::cout << datetime() << " " << j_data.toStyledString() << std::endl;
	}
#endif

}


/************************************************************************
*									*
* Message handler							*
*  - handles notifications sent by Harmony Hub				*
*									*
************************************************************************/
void HarmonyHubWS::ProcessHarmonyNotification(const Json::Value &j_data)
{
	// inform worker that a notification was received (advance ping if needed)
	m_bReceivedNotification = true;

	std::string msgtype = j_data["type"].asString();
	size_t loc = msgtype.find_last_of('.');
	std::string shorttype = msgtype.substr(loc+1);

	if (shorttype == "stateDigest?notify")
	{
		if (!j_data.isMember("data"))
		{
			_log.Log(LOG_ERROR, "Harmony Hub: invalid data");
			return;
		}

		int activityStatus;

		if (j_data["data"].isMember("activityStatus"))
		{
			activityStatus = atoi(j_data["data"]["activityStatus"].asString().c_str());

			bool bIsChanging = (activityStatus & 1);
			if (bIsChanging != m_bIsChangingActivity)
			{
				m_bIsChangingActivity = bIsChanging;
				if (m_bIsChangingActivity)
					_log.Log(LOG_STATUS, "Harmony Hub: Changing activity");
				else
					_log.Log(LOG_STATUS, "Harmony Hub: Finished changing activity");
			}
		}

		if (_log.IsDebugLevelEnabled(DEBUG_HARDWARE) || (m_szHubSwVersion.empty()))
		{
			if (j_data["data"].isMember("hubSwVersion"))
			{
				std::string szHubSwVersion = j_data["data"]["hubSwVersion"].asString();
				if (m_szHubSwVersion != szHubSwVersion)
					_log.Log(LOG_STATUS, "Harmony Hub: Software version: %s", szHubSwVersion.c_str());
				m_szHubSwVersion = szHubSwVersion;
			}
		}

		if (_log.IsDebugLevelEnabled(DEBUG_HARDWARE))
		{
			std::string activityId, stateVersion;
			if (j_data["data"].isMember("runningActivityList"))
			{
				activityId = j_data["data"]["runningActivityList"].asString();
				if (activityId.empty())
					activityId = "-1";
			}
			else
				activityId = "NaN";

			if (j_data["data"].isMember("stateVersion"))
				stateVersion = j_data["data"]["stateVersion"].asString();
			if (stateVersion.empty())
				stateVersion = "NaN";

			_log.Debug(DEBUG_HARDWARE, "Harmony Hub: Event state notification: stateVersion = %s, hubSwVersion = %s, activityStatus = %d, activityId = %s", stateVersion.c_str(), m_szHubSwVersion.c_str(), activityStatus, activityId.c_str() );
		}
	}

	else if ((shorttype == "engine?startActivityFinished") || (shorttype == "engine?helpdiscretes"))
	{
		std::string szActivityId = j_data["data"]["activityId"].asString();
		if (szActivityId != m_szCurActivityID)
		{
			CheckSetActivity(m_szCurActivityID, false);
			m_szCurActivityID = szActivityId;
			CheckSetActivity(m_szCurActivityID, true);
		}

	}

#ifdef _DEBUG
	else if (shorttype == "button?pressType")
	{
		std::cout << datetime() << " button pressed (type = " << j_data["data"]["type"].asString() << ")\n";
	}
	else
	{
		// unhandled
		std::cout << datetime() << " unhandled notification:\n";
		std::cout << j_data.toStyledString() << std::endl;
	}
#endif
}


/************************************************************************
*									*
* Async receiver							*
*  - handles messages sent by Harmony Hub				*
*									*
************************************************************************/

void HarmonyHubWS::AsyncReceiver(const std::string szdata)
{
	Json::Value j_result;

	bool ret = ParseJSon(szdata.c_str(), j_result);

	if ((!ret) || (!j_result.isObject()))
	{
		_log.Log(LOG_ERROR, "Harmony Hub: invalid json object");
		return;
	}

	if (!j_result.isMember("code"))
	{
		// hub sent us a notification
		ProcessHarmonyNotification(j_result);
		return;
	}

	// hub responds to something we sent
	m_bWantAnswer = false;
	int returncode = atoi(j_result["code"].asString().c_str());

	if (returncode >= 10000)
	{
		// curl status code
		returncode -= 10000; // curl return codes have 10000 added to them

		if (m_connectionstatus == harmonyhubpp::connection::status::connecting)
		{
			if (returncode == 0)
			{
				m_connectionstatus = harmonyhubpp::connection::status::connected;
				m_bShowConnectError = false;
				_log.Log(LOG_STATUS, "Harmony Hub: Connected to Hub");
				return;
			}
			else
			{
				m_connectionstatus = harmonyhubpp::connection::status::closed;
				if (m_bShowConnectError)
					_log.Log(LOG_ERROR, "Harmony Hub: Attempt to connect to Hub returned HTTP client error %d (%s)", returncode, j_result["msg"].asString().c_str());
				else
					_log.Debug(DEBUG_HARDWARE, "Harmony Hub: Attempt to connect to Hub returned HTTP client error %d (%s)", returncode, j_result["msg"].asString().c_str());
			}
			return;
		}

		_log.Log(LOG_ERROR, "Harmony Hub: Unexpected HTTP client error %d (%s)", returncode, j_result["msg"].asString().c_str());
		return;
	}

	if (returncode >= 1000)
	{
		// websocket connection ended
		if ((m_connectionstatus == harmonyhubpp::connection::status::closing) && (returncode == 1000))
		{
			// okay, we initiated this
			_log.Log(LOG_STATUS, "Harmony Hub: Connection closed");
		}
		else
		{
			_log.Log(LOG_ERROR, "Harmony Hub: Connection closed unexpectedly (error = %d)", returncode);
		}
		m_connectionstatus = harmonyhubpp::connection::status::closed;
		return;
	}

	ProcessHarmonyResponse(j_result);
	return;
}

}; //namespace handler
}; //namespace hardware
