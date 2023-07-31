/*
 * Copyright (c) 2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Base class for accessing web content
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include "RESTClient.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "main/Logger.h"


/************************************************************************
 *									*
 * Init									*
 *									*
 ************************************************************************/

bool		RESTClient::m_bCurlGlobalInitialized = false;
bool		RESTClient::m_bVerifyHost = false;
bool		RESTClient::m_bVerifyPeer = false;
long		RESTClient::m_iConnectionTimeout = 10;
long		RESTClient::m_iTimeout = 90;
std::string	RESTClient::m_sUserAgent = "curle/1.0";
std::string	RESTClient::m_sCookieFile = "cookie.txt";


/************************************************************************
 *									*
 * Configuration functions						*
 *									*
 ************************************************************************/

void RESTClient::SetConnectionTimeout(const long timeout)
{
	m_iConnectionTimeout = timeout;
}

void RESTClient::SetTimeout(const long timeout)
{
	m_iTimeout = timeout;
}

void RESTClient::SetSecurityOptions(const bool verifypeer, const bool verifyhost)
{
	m_bVerifyPeer = verifypeer;
	m_bVerifyHost = verifyhost;
}

void RESTClient::SetUserAgent(const std::string &useragent)
{
	m_sUserAgent = useragent;
}


void RESTClient::SetCookieFile(const std::string &cookiefile)
{
	m_sCookieFile = cookiefile;
}

/************************************************************************
 *									*
 * Curl callback writer functions					*
 *									*
 ************************************************************************/

namespace connection {
  namespace HTTP {
    namespace callback {

      size_t write_curl_headerdata(void *contents, size_t size, size_t nmemb, void *userp) // called once for each header
      {
	size_t realsize = size * nmemb;
	std::vector<std::string>* pvHeaderData = (std::vector<std::string>*)userp;
	pvHeaderData->push_back(std::string((unsigned char*)contents, (std::find((unsigned char*)contents, (unsigned char*)contents + realsize, '\r'))));
	return realsize;
      }

      size_t write_curl_data(void *contents, size_t size, size_t nmemb, void *userp)
      {
	size_t realsize = size * nmemb;
	std::vector<unsigned char>* pvHTTPResponse = (std::vector<unsigned char>*)userp;
	pvHTTPResponse->insert(pvHTTPResponse->end(), (unsigned char*)contents, (unsigned char*)contents + realsize);
	return realsize;
      }

      size_t write_curl_data_file(void *contents, size_t size, size_t nmemb, void *userp)
      {
	size_t realsize = size * nmemb;
	std::ofstream *outfile = (std::ofstream*)userp;
	outfile->write((const char*)contents, realsize);
	return realsize;
      }

      size_t write_curl_data_single_line(void *contents, size_t size, size_t nmemb, void *userp)
      {
	size_t realsize = size * nmemb;
	std::vector<unsigned char>* pvHTTPResponse = (std::vector<unsigned char>*)userp;
	size_t ii=0;
	while (ii< realsize)
	{
		unsigned char *pData = (unsigned char*)contents + ii;
		if ((pData[0] == '\n') || (pData[0] == '\r'))
			return 0;
		pvHTTPResponse->push_back(pData[0]);
		ii++;
	}
	return realsize;
      }

    }; // namespace callback

    typedef struct _tStatusCodes {
	const uint16_t httpCode;
	const char* szDescription;
    } StatusCodes;

    const StatusCodes httpErrors[] = {
	{ 400, "Bad Request" },
	{ 401, "Unauthorized" },
	{ 402, "Payment Required" },
	{ 403, "Forbidden" },
	{ 404, "Not Found" },
	{ 405, "Method Not Allowed" },
	{ 406, "Not Acceptable" },
	{ 407, "Proxy Authentication Required" },
	{ 408, "Request Timeout" },
	{ 409, "Conflict" },
	{ 410, "Gone" },
	{ 411, "Length Required" },
	{ 412, "Precondition Failed" },
	{ 413, "Request Entity Too Large" },
	{ 414, "Request - URI Too Long" },
	{ 415, "Unsupported Media Type" },
	{ 416, "Requested Range Not Satisfiable" },
	{ 417, "Expectation Failed" },
	{ 418, "I'm a teapot (RFC 2324)" },
	{ 420, "Enhance Your Calm(Twitter)" },
	{ 422, "Unprocessable Entity(WebDAV)" },
	{ 423, "Locked(WebDAV)" },
	{ 424, "Failed Dependency(WebDAV)" },
	{ 425, "Reserved for WebDAV" },
	{ 426, "Upgrade Required" },
	{ 428, "Precondition Required" },
	{ 429, "Too Many Requests" },
	{ 431, "Request Header Fields Too Large" },
	{ 444, "No Response(Nginx)" },
	{ 449, "Retry With(Microsoft)" },
	{ 450, "Blocked by Windows Parental Controls(Microsoft)" },
	{ 451, "Unavailable For Legal Reasons" },
	{ 499, "Client Closed Request(Nginx)" },
	{ 500, "Internal Server Error" },
	{ 501, "Not Implemented" },
	{ 502, "Bad Gateway" },
	{ 503, "Service Unavailable" },
	{ 504, "Gateway Timeout" },
	{ 505, "HTTP Version Not Supported" },
	{ 506, "Variant Also Negotiates(Experimental)" },
	{ 507, "Insufficient Storage(WebDAV)" },
	{ 508, "Loop Detected(WebDAV)" },
	{ 509, "Bandwidth Limit Exceeded(Apache)" },
	{ 510, "Not Extended" },
	{ 511, "Network Authentication Required" },
	{ 598, "Network read timeout error" },
	{ 599, "Network connect timeout error" },
	{ 0, nullptr }
    };

  }; // namespace HTTP
}; // namespace connection


/************************************************************************
 *									*
 * Private functions							*
 *									*
 ************************************************************************/

bool RESTClient::CheckIfGlobalInitDone()
{
	if (!m_bCurlGlobalInitialized)
	{
		CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
		if (res != CURLE_OK)
			return false;
		m_bCurlGlobalInitialized = true;
	}
	return true;
}

void RESTClient::Cleanup()
{
	if (m_bCurlGlobalInitialized)
	{
		curl_global_cleanup();
	}
}

void RESTClient::SetGlobalOptions(void *curlobj)
{
	CURL *curl=(CURL *)curlobj;
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC | CURLAUTH_DIGEST);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, m_sUserAgent.c_str());
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, m_iConnectionTimeout);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_iTimeout);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, m_bVerifyPeer ? 1L : 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, m_bVerifyHost ? 2L : 0);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, m_sCookieFile.c_str());
	curl_easy_setopt(curl, CURLOPT_COOKIEJAR, m_sCookieFile.c_str());
}

void RESTClient::LogStatus(const long responseCode)
{
	const connection::HTTP::StatusCodes* pCodes = (const connection::HTTP::StatusCodes*)&connection::HTTP::httpErrors;

	while (pCodes[0].httpCode != 0)
	{
		if (pCodes[0].httpCode == responseCode)
		{
			_log.Debug(DEBUG_NORM, "HTTP %d: %s", pCodes[0].httpCode, pCodes[0].szDescription);
			return;
		}
		pCodes++;
	}
	_log.Debug(DEBUG_NORM, "HTTP return code is: %li", responseCode);
}

/************************************************************************
 *									*
 * main method								*
 *									*
 * Note:								*
 * for DOWNLOAD method the target file name must be in szResponse	*
 * (vResponse[0] for binary method)					*
 *									*
 ************************************************************************/

bool RESTClient::ExecuteBinary(const connection::HTTP::method::value eMethod, const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse, std::vector<std::string> &vHeaderData, const bool bFollowRedirect, const long iTimeOut)
{
	try
	{
		if (!CheckIfGlobalInitDone())
			return false;
		CURL *curl = curl_easy_init();
		if (!curl)
			return false;

		CURLcode res;
		SetGlobalOptions(curl);
		if (iTimeOut != -1)
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, iTimeOut);
		if (!bFollowRedirect)
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);

		struct curl_slist *headers = nullptr;
		if (vExtraHeaders.size() > 0)
		{
			std::vector<std::string>::const_iterator itt;
			for (itt = vExtraHeaders.begin(); itt != vExtraHeaders.end(); ++itt)
			{
				headers = curl_slist_append(headers, (*itt).c_str());
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		}

		if (eMethod & connection::HTTP::method::HEAD)
		{
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, connection::HTTP::callback::write_curl_headerdata);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &vHeaderData);
		}

		std::ofstream outfile;
		if (eMethod == connection::HTTP::method::HEAD)
		{
			curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
		}
		else if (eMethod & connection::HTTP::method::DOWNLOAD)
		{
			// vResponse[0] contains the output file name
			std::string szFilename;
			szFilename.insert(szFilename.begin(), vResponse.begin(), vResponse.end());
			outfile.open(szFilename.c_str(), std::ios::out|std::ios::binary|std::ios::trunc);
			if (!outfile.is_open())
				return false;
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, connection::HTTP::callback::write_curl_data_file);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&outfile);
		}
		else
		{
			if (eMethod & connection::HTTP::method::GETSINGLELINE)
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, connection::HTTP::callback::write_curl_data_single_line);
			else 
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, connection::HTTP::callback::write_curl_data);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&vResponse);

			if ((int)eMethod & (connection::HTTP::method::POST | connection::HTTP::method::PUT | connection::HTTP::method::DELETE | connection::HTTP::method::PATCH))
			{
				if (eMethod & connection::HTTP::method::POST)
					curl_easy_setopt(curl, CURLOPT_POST, 1);
				else if (eMethod & connection::HTTP::method::PUT)
					curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
				else if (eMethod & connection::HTTP::method::DELETE)
					curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
				else if (eMethod & connection::HTTP::method::PATCH)
					curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, szPostdata.c_str());
			}
			else if (eMethod & connection::HTTP::method::OPTIONS)
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
		}

		curl_easy_setopt(curl, CURLOPT_URL, szUrl.c_str());
		res = curl_easy_perform(curl);

		if (res != CURLE_HTTP_RETURNED_ERROR)
		{
			// create a custom header
			std::stringstream ss;
			ss << "CURLE " << res << " " << curl_easy_strerror(res);
			vHeaderData.push_back(ss.str());
		}

		curl_easy_cleanup(curl);

		if (headers != nullptr)
			curl_slist_free_all(headers);

		if (eMethod & connection::HTTP::method::DOWNLOAD)
			outfile.close(); 

		return (res == CURLE_OK);
	}
	catch (...)
	{
		// create a custom header
		vHeaderData.push_back("CURLE -1 Exception in HTTP client");
		return false;
	}
}

bool RESTClient::Execute(const connection::HTTP::method::value eMethod, const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bFollowRedirect, const long iTimeOut, const bool bIgnoreNoDataReturned)
{

	std::vector<unsigned char> vResponse;
	if (eMethod & connection::HTTP::method::DOWNLOAD)
		vResponse.insert(vResponse.begin(), szResponse.begin(), szResponse.end());

	szResponse = "";
	if (!ExecuteBinary(eMethod, szUrl, szPostdata, vExtraHeaders, vResponse, vHeaderData, bFollowRedirect, iTimeOut))
		return false;
	if (!bIgnoreNoDataReturned && vResponse.empty())
		return false;
	szResponse.insert(szResponse.begin(), vResponse.begin(), vResponse.end());
	return true;
}


