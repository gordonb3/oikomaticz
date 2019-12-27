#include "stdafx.h"
#include "WebsocketHandler.h"
#include "main/localtime_r.h"
#include "main/json_helper.h"
#include "cWebem.h"
#include "main/Logger.h"

#define WEBSOCKET_SESSION_TIMEOUT 86400 // 1 day

namespace http {
	namespace server {

		CWebsocketHandler::CWebsocketHandler(cWebem *pWebem, boost::function<void(const std::string &packet_data)> _MyWrite) : 
			m_Push(this),
			sessionid(""),
			MyWrite(_MyWrite),
			myWebem(pWebem)
		{
			
		}

		CWebsocketHandler::~CWebsocketHandler()
		{
			Stop();
		}

		boost::tribool CWebsocketHandler::Handle(const std::string &packet_data, bool outbound)
		{
			Json::Value jsonValue;

			try
			{
				// WebSockets only do security during set up so keep pushing the expiry out to stop it being cleaned up
				WebEmSession session;
				std::map<std::string, WebEmSession>::iterator itt = myWebem->m_sessions.find(sessionid);
				if (itt != myWebem->m_sessions.end())
				{
					session = itt->second;
				}
				else
					// for outbound messages create a temporary session if required
					// todo: Add the username and rights from the original connection
					if (outbound)
					{
							time_t	nowAnd1Day = ((time_t)mytime(NULL)) + WEBSOCKET_SESSION_TIMEOUT;
							session.timeout = nowAnd1Day;
							session.expires = nowAnd1Day;
							session.isnew = false;
							session.forcelogin = false;
							session.rememberme = false;
							session.reply_status = 200;
					}


				Json::Value value;
				if (!ParseJSon(packet_data, value)) {
					return true;
				}
				if (value["event"] != "request") {
					return true;
				}
				request req;
				req.method = "GET";
				std::string querystring = value["query"].asString();
				req.uri = "/json.htm?" + querystring;
				req.http_version_major = 1;
				req.http_version_minor = 1;
				req.headers.resize(0); // todo: do we need any headers?
				req.content.clear();
				reply rep;
				if (myWebem->CheckForPageOverride(session, req, rep)) {
					if (rep.status == reply::ok) {
						jsonValue["event"] = "response";
						jsonValue["requestid"] = value["requestid"].asInt64();
						jsonValue["data"] = rep.content;
						std::string response = JSonToFormatString(jsonValue);
						MyWrite(response);
						return true;
					}
				}
			}
			catch (std::exception& e)
			{
				_log.Log(LOG_ERROR, "WebsocketHandler::%s Exception: %s", __func__, e.what());
			}

			jsonValue["error"] = "Internal Server Error!!";
			std::string response = JSonToFormatString(jsonValue);
			MyWrite(response);
			return true;
		}

		void CWebsocketHandler::Start()
		{
			m_Push.Start();
		}

		void CWebsocketHandler::Stop()
		{
			m_Push.Stop();
		}


		// todo: not sure 
		void CWebsocketHandler::store_session_id(const request &req, const reply &rep)
		{
			//Check cookie if still valid
			const char* cookie_header = request::get_req_header(&req, "Cookie");
			if (cookie_header != NULL)
			{
				std::string sSID;
				std::string szTime;

				// Parse session id and its expiration date
				std::string scookie = cookie_header;
				size_t fpos = scookie.find("DMZSID=");
				if (fpos != std::string::npos)
				{
					scookie = scookie.substr(fpos);
					fpos = 0;
					size_t epos = scookie.find(';');
					if (epos != std::string::npos)
					{
						scookie = scookie.substr(0, epos);
					}
				}
				size_t upos = scookie.find("_", fpos);
				size_t ppos = scookie.find(".", upos);
				time_t now = mytime(NULL);
				if ((fpos != std::string::npos) && (upos != std::string::npos) && (ppos != std::string::npos))
				{
					sSID = scookie.substr(fpos + 7, upos - fpos - 7);
					//std::string sAuthToken = scookie.substr(upos + 1, ppos - upos - 1);
					szTime = scookie.substr(ppos + 1);

					time_t stime;
					std::stringstream sstr;
					sstr << szTime;
					sstr >> stime;

					bool expired = stime < now;
					if (!expired) {
						sessionid = sSID;
					}
				}
			}
		}

		void CWebsocketHandler::OnDeviceChanged(const uint64_t DeviceRowIdx)
		{
			//Rob, needed a try/catch, but don't know why...
			//When a browser was still open and polling/connecting to the websocket, and the application was started this caused a crash
			try
			{
				std::string query = "type=devices&rid=" + std::to_string(DeviceRowIdx);
				Json::Value request;
				request["event"] = "request";
				request["requestid"] = -1;
				request["query"] = query;
				std::string packet = JSonToFormatString(request);
				Handle(packet, true);
			}
			catch (std::exception& e)
			{
				_log.Log(LOG_ERROR, "WebsocketHandler::%s Exception: %s", __func__, e.what());
			}
		}

		void CWebsocketHandler::OnMessage(const std::string &Subject, const std::string &Text, const std::string &ExtraData, const int Priority, const std::string &Sound, const bool bFromNotification)
		{
			Json::Value json;

			json["event"] = "notification";
			json["Subject"] = Subject;
			json["Text"] = Text;
			json["ExtraData"] = ExtraData;
			json["Priority"] = Priority;
			json["Sound"] = Sound;
			json["bFromNotification"] = bFromNotification;
			std::string response = json.toStyledString();
			MyWrite(response);
		}

	}
}
