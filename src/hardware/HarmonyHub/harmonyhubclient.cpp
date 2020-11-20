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

#include "harmonyhubclient.hpp"


#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

#include <iostream>

#ifndef without_discovery
  #include <curl/curl.h>
#endif


#ifndef _WIN32
#define sprintf_s(buffer, buffer_size, stringbuffer, ...) (sprintf(buffer, stringbuffer, __VA_ARGS__))
#endif


#define DEFAULTDOMAIN "svcs.myharmony.com"


namespace harmonyhubpp {

#ifndef without_discovery
/************************************************************************
 *									*
 * harmonyhubpp::HarmonyDiscovery					*
 *									*
 * This subclass performs a HTTP call to the HarmonyHub to retrieve	*
 * parameters needed for our websockets connection			*
 *									*
 * Methods:								*
 *	bool is_msgOK()							*
 *	std::string get_id()						*
 *	std::string get_domain()					*
 *	std::string get_raw_message()					*
 *	( std::string get_fw_version() )				*
 *									*
 * It is recommended that you store the ID and Domain name for		*
 * future calls to this library						*
 *									*
 ***********************************************************************/
class HarmonyDiscovery
{
private:
	// Curl write back function
	static size_t write_curl_data(void *contents, size_t size, size_t nmemb, void *userdata)
	{
		size_t realsize = size * nmemb;
		std::vector<unsigned char>* pvHTTPResponse = (std::vector<unsigned char>*)userdata;
		pvHTTPResponse->insert(pvHTTPResponse->end(), (unsigned char*)contents, (unsigned char*)contents + realsize);
		return realsize;
	}

	std::string get_strvalue(size_t offset)
	{
		size_t start = m_discoveryinfo.find_first_not_of(" :", offset);
		size_t stop;
		if (m_discoveryinfo[start] == '"')
		{
			start++;
			stop = m_discoveryinfo.find_first_of('"', start);
		}
		else
			stop = m_discoveryinfo.find_first_of(" ,}", start + 1);

		if (stop != std::string::npos)
			return m_discoveryinfo.substr(start, stop - start);
		return "";
	}

	bool HTTP_POST(const std::string uri, const std::string origin, const std::string postdata)
	{
		m_discoveryinfo = "";
		std::vector<unsigned char> response;
		CURL *conn;
		CURLcode curlres;

		curl_global_init(CURL_GLOBAL_ALL);

		conn = curl_easy_init();
		if (!conn)
		{
			m_discoveryinfo = "{\"msg\":\"Failed to instantiate libcurl\",\"code\":\"-1\"}";
			return false;
		}

		curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
		std::string m_userAgent = "libcurl-agent/";
		m_userAgent.append(info->version);

		struct curl_slist *httpheader = nullptr;
		httpheader = curl_slist_append(httpheader, "Content-Type: application/json");
		httpheader = curl_slist_append(httpheader, "Accept : application/json");
		httpheader = curl_slist_append(httpheader, "charsets: utf-8");
		httpheader = curl_slist_append(httpheader, origin.c_str());

		curl_easy_reset(conn);
		curl_easy_setopt(conn, CURLOPT_USERAGENT, m_userAgent.c_str());
		curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, this->write_curl_data);
		curl_easy_setopt(conn, CURLOPT_HTTPHEADER, httpheader);
		curl_easy_setopt(conn, CURLOPT_WRITEDATA, (void *)&response);
		curl_easy_setopt(conn, CURLOPT_POSTFIELDS, postdata.c_str());
		curl_easy_setopt(conn, CURLOPT_URL, uri.c_str());
		curlres = curl_easy_perform(conn);

		if (curlres == CURLE_OK)
		{
			m_discoveryinfo.insert(m_discoveryinfo.begin(),response.begin(),response.end());
		}
		else
		{
			m_discoveryinfo = "{\"msg\":\"";
			m_discoveryinfo.append(curl_easy_strerror(curlres));
			m_discoveryinfo.append("\",\"code\":\"");
			m_discoveryinfo.append(std::to_string(curlres + 10000));
			m_discoveryinfo.append("\"}");
		}
		curl_slist_free_all(httpheader);
		curl_easy_cleanup(conn);
		curl_global_cleanup();

		return (curlres == CURLE_OK);
	}


public:
	HarmonyDiscovery(const std::string IPAddress)
	{
		std::string uri = "http://";
		uri.append(IPAddress);
		uri.append(":8088");

		std::string origin = "Origin: http://localhost.nebula.myharmony.com";
		std::string postdata = "{\"id\":1,\"cmd\":\"connect.discoveryinfo?get\",\"params\":{}}";
		if (!HTTP_POST(uri, origin, postdata))
			return;

		if (!is_msgOK() && (m_discoveryinfo.find("417") != std::string::npos))
		{
			// discovery returned code 417 -> retry the call with different parameters

			origin = "Origin: http://sl.dhg.myharmony.com";
			postdata = "{\"id\":1,\"cmd\":\"setup.account?getProvisionInfo\",\"params\":{}}";
			HTTP_POST(uri, origin, postdata);
		}
	}

	~HarmonyDiscovery()
	{
	}

	bool is_msgOK()
	{
		size_t offset = m_discoveryinfo.find("\"OK\"");
		if (offset == std::string::npos)
			return false;
		return true;
	}

	std::string get_id()
	{
		size_t offset = m_discoveryinfo.find("emoteId");
		if (offset == std::string::npos)
			return "";
		return get_strvalue(offset + 9);
	}

	std::string get_domain()
	{
		size_t offset = m_discoveryinfo.find("discoveryServerUri\"");
		if (offset != std::string::npos)
			offset += 20;
		else
		{
			offset = m_discoveryinfo.find("discoveryServer\"");
			if (offset == std::string::npos)
				return "";
			offset += 17;
		}
		std::string domain = get_strvalue(offset);
		size_t start = domain.find_first_of(":\\/") + 1;
		size_t stop = domain.find_first_of("\\/", start);
		while ((stop == start) && (stop != std::string::npos))
		{
			start++;
			stop = domain.find_first_of("\\/", start);
		}
		return domain.substr(start, stop - start);
	}

	std::string get_raw_message()
	{
		return m_discoveryinfo;
	}

/*
	std::string get_fw_version()
	{
		size_t offset = m_discoveryinfo.find("current_fw_version");
		return get_strvalue(offset + 20);
	}
*/

private:
	std::string m_discoveryinfo;
}; // class discovery
#endif



/****************************************************************
 *								*
 * harmonyhubpp::HarmonyConnection				*
 *								*
 * This subclass provides the link with websocketpp		*
 *								*
 ****************************************************************/
class HarmonyConnection
{
	typedef websocketpp::client<websocketpp::config::asio_client> _wsclient;

public:
	HarmonyConnection ()
	{
		// disable logging
		m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
		m_endpoint.clear_error_channels(websocketpp::log::elevel::all);

		// Start ASIO thread
		m_endpoint.init_asio();
		m_endpoint.start_perpetual();
		m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&_wsclient::run, &m_endpoint);

		// set defaults
		m_status = harmonyhubpp::session::status::connecting;
	}

	~HarmonyConnection() {
		m_endpoint.stop_perpetual();

		if (m_status == harmonyhubpp::session::status::open)
		{
			websocketpp::lib::error_code ec;
			m_endpoint.close(m_hdl, websocketpp::close::status::going_away, "", ec);
			if (ec)
			{
				// std::cerr << "> Error closing connection: " << ec.message() << std::endl;
			}
		}

		m_thread->join();
	}

	std::error_code connect(const std::string &ip_address, std::string &remote_id, std::string &domain)
	{
		m_status = harmonyhubpp::session::status::connecting;

		if (remote_id.empty())
		{
#ifdef without_discovery
			return std::make_error_code(std::errc::invalid_argument);
#else
			harmonyhubpp::HarmonyDiscovery *HubInfo = new harmonyhubpp::HarmonyDiscovery(ip_address);
			if (!HubInfo->is_msgOK())
			{
				std::string message = HubInfo->get_raw_message();
				delete HubInfo;
				if (!m_message_handler)
				{
					size_t offset = message.find("\"code\"");
					offset += 7;
					while (message[offset] != '"')
						offset++;
					std::cout << "> (";
					offset++;
					while (message[offset] != '"')
						std::cout << message[offset];
					std::cout << ") ";
					offset = message.find("\"msg\"");
					offset += 6;
					while (message[offset] != '"')
						offset++;
					offset++;
					while (message[offset] != '"')
						std::cout << message[offset];
					std::cout << std::endl;
				}
				else
					m_message_handler(message);

				return std::make_error_code(std::errc::invalid_argument);
			}
			remote_id = HubInfo->get_id();
			domain = HubInfo->get_domain();
			delete HubInfo;
#endif
		}

		if (m_uri.empty() || !ip_address.empty())
		{
			m_uri = "ws://";
			m_uri.append(ip_address);
			m_uri.append(":8088/?domain=");
			m_uri.append(domain);
			m_uri.append("&hubId=");
			m_uri.append(remote_id);
		}

		websocketpp::lib::error_code ec;
		con = m_endpoint.get_connection(m_uri, ec);;
		if (ec)
			return ec;

		m_hdl = con->get_handle();

		con->set_open_handler(websocketpp::lib::bind(&HarmonyConnection::on_open, this));
		con->set_fail_handler(websocketpp::lib::bind(&HarmonyConnection::on_fail, this));
		con->set_close_handler(websocketpp::lib::bind(&HarmonyConnection::on_close, this));
		con->set_message_handler(websocketpp::lib::bind(&HarmonyConnection::on_message, this, websocketpp::lib::placeholders::_2));

		m_endpoint.connect(con);
		return ec;
	}

	std::error_code close(std::string reason)
	{
		if (m_status != harmonyhubpp::session::status::open)
			return std::make_error_code(std::errc::not_connected);

		websocketpp::lib::error_code ec;
		m_endpoint.close(m_hdl, websocketpp::close::status::normal, reason, ec);
		return ec;
	}

	std::error_code send(std::string message)
	{
		if (m_status == harmonyhubpp::session::status::closed)
		{
			std::string res = "";
			connect("", res, res);
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
			if (m_status != harmonyhubpp::session::status::open)
				return std::make_error_code(std::errc::not_connected);
		}

		websocketpp::lib::error_code ec;
		m_endpoint.send(m_hdl, message, websocketpp::frame::opcode::text, ec);
		return ec;
	}


	void set_message_handler(harmonyhubpp::message_handler h)
	{
		m_message_handler = h;
	}


	/****************************************************************
	 *								*
	 * Callbacks							*
	 *								*
	 ****************************************************************/

	void on_open()
	{
		m_status = harmonyhubpp::session::status::open;

		if (!m_message_handler)
			std::cout << "> Connected to Harmony Hub" << std::endl;
		else
			m_message_handler("{\"code\":10000,\"msg\":\"OK\"}");
	}

	void on_fail()
	{
		m_status = harmonyhubpp::session::status::failed;

		std::string msg = "{\"code\":";
		msg.append(std::to_string(con->get_ec().value() + 10000));
		msg.append(",\"msg\":\"");
		msg.append(con->get_ec().message());
		msg.append("\"}");

		if (!m_message_handler)
		{
			if (m_status == harmonyhubpp::session::status::connecting)
				std::cout << "> Failed to connect to Harmony Hub: ";
			std::cout << msg << "\n";
		}
		else
			m_message_handler(msg);
	}

	void on_close()
	{
		m_status = harmonyhubpp::session::status::closed;

		std::string msg = "{\"code\":";
		msg.append(std::to_string(con->get_remote_close_code()));
		msg.append(",\"msg\":\"");
		msg.append(websocketpp::close::status::get_string(con->get_remote_close_code()));
		msg.append("\"}");

		if (!m_message_handler)
			std::cout << msg << "\n";
		else
			m_message_handler(msg);
	}

	void on_message(_wsclient::message_ptr msg)
	{
		if (!m_message_handler)
			std::cout << msg->get_payload() << "\n";
		else
			m_message_handler(msg->get_payload());
	}




private:
	_wsclient m_endpoint;
	_wsclient::connection_ptr con;

	websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
	websocketpp::connection_hdl m_hdl;

	harmonyhubpp::message_handler m_message_handler;

	std::string m_uri;
	std::string m_error_reason;

	harmonyhubpp::session::status::value m_status;


}; // class connection



/****************************************************************
 *								*
 * Main class entry point					*
 *								*
 ****************************************************************/
HarmonyClient::HarmonyClient()
{
	m_conn = new harmonyhubpp::HarmonyConnection();
	m_domain = DEFAULTDOMAIN;
	m_msgid = 0;
}

HarmonyClient::~HarmonyClient()
{
	delete m_conn;
}

void HarmonyClient::set_hubid(const std::string remoteID, const std::string domain)
{
	m_remoteID = remoteID;
	if (!domain.empty())
		m_domain = domain;
}

std::string HarmonyClient::get_hubid()
{
	return m_remoteID;
}

std::string HarmonyClient::get_domain()
{
	return m_domain;
}

std::error_code HarmonyClient::connect(const std::string IPAddress)
{
	return m_conn->connect(IPAddress, m_remoteID, m_domain);
}

std::error_code HarmonyClient::close(const std::string reason)
{
	return m_conn->close(reason);
}

void HarmonyClient::set_message_handler(harmonyhubpp::message_handler h)
{
	m_conn->set_message_handler(h);
}

std::error_code HarmonyClient::send_request(const std::string cmd, const std::string params)
{
	m_msgid++;
	std::string message = "{\"hubId\":\"";
	message.append(m_remoteID);
	message.append("\",\"timeout\":30,\"hbus\":{\"cmd\":\"");
	message.append(cmd);
	message.append("\",\"id\":\"");
	message.append(std::to_string(m_msgid));
	message.append("\",\"params\":");
	if (params.empty())
		message.append("{\"verb\":\"get\",\"format\":\"json\"}");
	else
		message.append(params);
	message.append("}}");

	return m_conn->send(message);
}


std::error_code HarmonyClient::get_statedigest()
{
	return send_request("connect.get_statedigest");
}

std::error_code HarmonyClient::ping()
{
	return send_request("connect.ping");
}

std::error_code HarmonyClient::get_current_activity()
{
	return send_request("harmony.engine?getCurrentActivity");
}

std::error_code HarmonyClient::get_config()
{
	return send_request("harmony.engine?config");
}

std::error_code HarmonyClient::get_activitylist()
{
	std::string params = "{\"uri\":\"harmony://Account/";
	params.append(m_remoteID);
	params.append("/ActivityList\"}");
	return send_request("proxy.resource?get", params);
}

std::error_code HarmonyClient::get_devicelist()
{
	std::string params = "{\"uri\":\"harmony://Account/";
	params.append(m_remoteID);
	params.append("/DeviceList\"}");
	return send_request("proxy.resource?get", params);
}

std::error_code HarmonyClient::get_capabilitylist()
{
	std::string params = "{\"uri\":\"harmony://Account/";
	params.append(m_remoteID);
	params.append("/CapabilityList\"}");
	return send_request("proxy.resource?get", params);
}

std::error_code HarmonyClient::get_homeautomation_config()
{
	return send_request("proxy.resource?get","{\"uri\":\"dynamite://HomeAutomationService/Config/\"}");
}

std::error_code HarmonyClient::get_userinfo()
{
	return send_request("setup.account?getProvisionInfo");
}

std::error_code HarmonyClient::start_activity(std::string activity_id)
{
	std::string params = "{\"activityId\":\"";
	params.append(activity_id);
	params.append("\",\"async\":\"true\",\"timestamp\":0,\"args\":{\"rule\":\"start\"}}");
	return send_request("harmony.activityengine?runactivity", params);
}

std::error_code HarmonyClient::send_action(const std::string action, const int delay_ms)
{
	std::string params = "{\"status\":\"press\",\"timestamp\":0,\"verb\":\"render\",\"action\":\"";
	params.append(action);
	params.append("\"}");

	std::error_code ec = send_request("harmony.engine?holdAction", params);
	if (ec)
		return ec;

	if (delay_ms > 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

	params.replace(11,5,"release");
	return send_request("harmony.engine?holdAction", params);
}

std::error_code HarmonyClient::send_device_command(const std::string device_id, const std::string cmd, const int delay_ms)
{
	std::string action = "{\\\"command\\\":\\\"";
	action.append(cmd);
	action.append("\\\",\\\"type\\\":\\\"IRCommand\\\",\\\"deviceId\\\":\\\"");
	action.append(device_id);
	action.append("\\\"}");

	return send_action(action, delay_ms);
}

std::error_code HarmonyClient::change_channel(int channel)
{
	std::string params = "{\"timestamp\":0,\"channel\":\"";
	params.append(std::to_string(channel));
	params.append("\"}");
	return send_request("harmony.engine?changeChannel", params);
}


std::error_code HarmonyClient::send_raw_message(const std::string message)
{
	return m_conn->send(message);
}

}; // namespace harmonyhubpp

