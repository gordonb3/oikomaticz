/*
 * Copyright (c) 2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Base class for accessing web content
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#pragma once
#include <string>
#include <vector>

namespace connection {
  namespace HTTP {

    namespace method {
	enum value
	{
		GET		= 0x0001,
		POST		= 0x0002,
		PUT		= 0x0004,
		DELETE		= 0x0008,
		HEAD		= 0x0010,
		PATCH		= 0x0020,
		OPTIONS		= 0x0040
	};
    }; // namespace method

  }; // namespace HTTP
}; // namespace connection




class RESTClient
{
protected:
	/************************************************************************
	 *									*
	 * cleanup function, should be called before application closes		*
	 *									*
	 ************************************************************************/
	
	static void Cleanup();


public:

	/************************************************************************
	 *									*
	 * Configuration functions						*
	 *									*
	 * CAUTION!								*
	 * Because these settings are global they may affect other classes	*
	 * that use RESTClient_Base or any class that extends it. Please add	*
	 * a debug line to your class if you access any of these functions.	*
	 *									*
	 ************************************************************************/
	
	static void SetConnectionTimeout(const long timeout);
	static void SetTimeout(const long timeout);
	static void SetUserAgent(const std::string &useragent);
	static void SetSecurityOptions(const bool verifypeer, const bool verifyhost);
	static void SetCookieFile(const std::string &cookiefile);


	/************************************************************************
	 *									*
	 * main method								*
	 *									*
	 ************************************************************************/

	static bool ExecuteBinary(const connection::HTTP::method::value eMethod, const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse, std::vector<std::string> &vHeaderData, const bool bFollowRedirect = true, const long iTimeOut = -1);
	static bool Execute(const connection::HTTP::method::value eMethod, const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bFollowRedirect = true, const long iTimeOut = -1, const bool bIgnoreNoDataReturned = false);


	/************************************************************************
	 *									*
	 * non public								*
	 *									*
	 ************************************************************************/

private:
	static void SetGlobalOptions(void *curlobj);
	static bool CheckIfGlobalInitDone();

private:
	static bool m_bCurlGlobalInitialized;
	static bool m_bVerifyHost;
	static bool m_bVerifyPeer;
	static long m_iConnectionTimeout;
	static long m_iTimeout;
	static std::string m_sUserAgent;
	static std::string m_sCookieFile;
};


