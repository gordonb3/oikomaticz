/*
 * Copyright (c) 2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Evohome HTTP bridge class
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include "EvoHTTPBridge.hpp"
#include <sstream>
#include <iomanip>

namespace evohome {
  namespace API {
    namespace method {
	enum value {
		GET	= (connection::HTTP::method::HEAD | connection::HTTP::method::GET),
		POST	= (connection::HTTP::method::HEAD | connection::HTTP::method::POST),
		PUT	= (connection::HTTP::method::HEAD | connection::HTTP::method::PUT),
		DELETE	= (connection::HTTP::method::HEAD | connection::HTTP::method::DELETE)
	};
    }; // namespace method
  }; // namespace API
}; // namespace evohome


bool EvoHTTPBridge::SafeGET(const std::string &szUrl, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const long iTimeOut)
{
	std::vector<std::string> vHeaderData;
	bool bhttpOK = Execute((connection::HTTP::method::value)evohome::API::method::GET, szUrl, "", vExtraHeaders, szResponse, vHeaderData, false, iTimeOut, true);
	return ProcessResponse(szResponse, vHeaderData, bhttpOK);
}

bool EvoHTTPBridge::SafePOST(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const long iTimeOut)
{
	std::vector<std::string> vHeaderData;
	bool bhttpOK = Execute((connection::HTTP::method::value)evohome::API::method::POST, szUrl, szPostdata, vExtraHeaders, szResponse, vHeaderData, false, iTimeOut, true);
	return ProcessResponse(szResponse, vHeaderData, bhttpOK);
}

bool EvoHTTPBridge::SafePUT(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const long iTimeOut)
{
	std::vector<std::string> vHeaderData;
	bool bhttpOK = Execute((connection::HTTP::method::value)evohome::API::method::PUT, szUrl, szPutdata, vExtraHeaders, szResponse, vHeaderData, false, iTimeOut, true);
	return ProcessResponse(szResponse, vHeaderData, bhttpOK);
}

bool EvoHTTPBridge::SafeDELETE(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const long iTimeOut)
{
	std::vector<std::string> vHeaderData;
	bool bhttpOK = Execute((connection::HTTP::method::value)evohome::API::method::DELETE, szUrl, szPutdata, vExtraHeaders, szResponse, vHeaderData, false, iTimeOut, true);
	return ProcessResponse(szResponse, vHeaderData, bhttpOK);
}

std::string EvoHTTPBridge::URLEncode(const std::string szDecodedString)
{
	char c;
	unsigned int i;
	std::stringstream ss;
	for (i=0; i < (unsigned int)szDecodedString.length(); i++)
	{
		c = szDecodedString[i];
		if (c == '-' || c == '_' || c == '.' || c == '~')
		{
			ss << c;
			continue;
		}
		if ( (c >= 0x30) && (c < 0x3A) )
		{
			ss << c;
			continue;
		}
		if ( ((c|0x20) > 0x60) && ((c|0x20) < 0x7b) )
		{
			ss << c;
			continue;
		}
		ss  << '%' << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << int((unsigned char) c) << std::nouppercase << std::dec;
	}
	return ss.str();
}

bool EvoHTTPBridge::ProcessResponse(std::string &szResponse, const std::vector<std::string> &vHeaderData, const bool bhttpOK)
{
	std::string szCode;
	std::string szMessage;

	if (vHeaderData.size() > 0)
	{
		size_t pos = vHeaderData[0].find(" ");
		if (pos != std::string::npos)
		{
			pos++;
			while (((vHeaderData[0][pos] & 0xF0) == 0x30) && ((vHeaderData[0][pos] & 0x0F) < 10))
			{
				szCode.append(1,vHeaderData[0][pos]);
				pos++;
			}
			pos++;
			if (pos < vHeaderData[0].size())
				szMessage = vHeaderData[0].substr(pos);
		}

		if (szCode.empty())
		{
			// fallthrough in case of an unexpected header content
			szCode = "-1";
			szMessage = vHeaderData[0];
		}

		if (!bhttpOK && (vHeaderData[0][0] == 'C'))
		{
			// use Curl error data to create a response
			szResponse = "{\"code\":\"";
			szResponse.append(szCode);
			szResponse.append("\",\"message\":\"");
			if (!szMessage.empty())
				szResponse.append(szMessage);
			else
			{
				szResponse.append("HTTP client error ");
				szResponse.append(szCode);
			}
                        szResponse.append("\"}");
			return bhttpOK;
		}

		if (!bhttpOK && !szResponse.empty())
		{
			if ((szResponse[0] == '[') || (szResponse[0] == '{'))
			{
				// append code to the json response so it will take preference over any existing (textual) message code
				size_t pos = szResponse.find_last_of("}");
				if (pos != std::string::npos)
				{
					szResponse.insert(pos, ",\"code\":\"\"");
					szResponse.insert(pos+9, szCode);
					return bhttpOK;
				}
			}
		}
	}

	if (szResponse.empty()) // use header data to create a response
	{
		if (szCode.empty())
		{
			szResponse = "{\"code\":\"204\",\"message\":\"Evohome portal did not return any data or status\"}";
			return true;
		}

		szResponse = "{\"code\":\"";
		szResponse.append(szCode);
		szResponse.append("\",\"message\":\"");
		if (!szMessage.empty())
			szResponse.append(szMessage);
		else
		{
			szResponse.append("HTTP ");
			szResponse.append(szCode);
		}
		szResponse.append("\"}");
		return bhttpOK;
	}

	if ((szResponse[0] == '[') || (szResponse[0] == '{')) // okay, appears to be json
		return bhttpOK;


	int i = static_cast<int>(szResponse.find("<title>"));
	if (i != std::string::npos) // extract the title from the returned HTML page
	{
		std::string szTemp = "{\"code\":\"";
		if (!szCode.empty())
			szTemp.append(szCode);
		else
			szTemp.append("-1");
		szTemp.append("\",\"message\":\"");
		i += 7;
		char c = szResponse[i];
		while ((c != '<') && (i < static_cast<int>(szResponse.size() - 1)))
		{
			szTemp.insert(szTemp.end(), 1, c);
			i++;
			c = szResponse[i];
		}
		szTemp.append("\"}");
		szResponse = szTemp;
		return bhttpOK;
	}

	if (szResponse.find("<html>") != std::string::npos) // received an HTML page without a title
	{
		std::string szTemp = "{\"code\":\"";
		if (!szCode.empty())
			szTemp.append(szCode);
		else
			szTemp.append("-1");
		szTemp.append("\",\"message\":\"");
		int i = 0;
		int maxchars = static_cast<int>(szResponse.size());
		char* html = &szResponse[0];
		char c;
		for (int i = 0; i < maxchars; i++)
		{
			c = html[i];
			if (c == '<')
			{
				while ((c != '>') && (i < (maxchars - 1)))
				{
					i++;
					c = html[i];
				}
			}
			else if (c != '<')
			{
				szTemp.insert(szTemp.end(), 1, c);
			}
		}
		szTemp.append("\"}");
		szResponse = szTemp;
		return bhttpOK;
	}

	// shouldn't get here
	szResponse = "{\"code\":\"-1\",\"message\":\"unhandled response\"}";
	return false;
}

void EvoHTTPBridge::CloseConnection()
{
	EvoHTTPBridge::Cleanup();
}

