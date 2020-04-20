/*
 * Copyright (c) 2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Extension class to HTTPClient_Base
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include "HTTPClient.h"


/************************************************************************
 *									*
 * binary methods with access to return header data			*
 *									*
 ************************************************************************/

bool HTTPClient::GETBinary(const std::string &szUrl, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse, std::vector<std::string> &vHeaderData, const long iTimeOut)
{
	return RESTClient::ExecuteBinary((connection::HTTP::method::value)(connection::HTTP::method::GET | connection::HTTP::method::HEAD), szUrl, "", vExtraHeaders, vResponse, vHeaderData, true, iTimeOut);
}

bool HTTPClient::GETBinarySingleLine(const std::string &szUrl, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse, std::vector<std::string> &vHeaderData, const long iTimeOut)
{
	return RESTClient::ExecuteBinary((connection::HTTP::method::value)(connection::HTTP::method::GETSINGLELINE | connection::HTTP::method::HEAD), szUrl, "", vExtraHeaders, vResponse, vHeaderData, true, iTimeOut);
}

bool HTTPClient::POSTBinary(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse, std::vector<std::string> &vHeaderData, const bool bFollowRedirect, const long iTimeOut)
{
	return RESTClient::ExecuteBinary((connection::HTTP::method::value)(connection::HTTP::method::POST | connection::HTTP::method::HEAD), szUrl, szPostdata, vExtraHeaders, vResponse, vHeaderData, bFollowRedirect, iTimeOut);
}


bool HTTPClient::PUTBinary(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse,
std::vector<std::string> &vHeaderData, const long iTimeOut)
{
	return RESTClient::ExecuteBinary((connection::HTTP::method::value)(connection::HTTP::method::PUT | connection::HTTP::method::HEAD), szUrl, szPutdata, vExtraHeaders, vResponse, vHeaderData, true, iTimeOut);
}


bool HTTPClient::DELETEBinary(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse,
std::vector<std::string> &vHeaderData, const long iTimeOut)
{
	return RESTClient::ExecuteBinary((connection::HTTP::method::value)(connection::HTTP::method::DELETE | connection::HTTP::method::HEAD), szUrl, szPutdata, vExtraHeaders, vResponse, vHeaderData, true, iTimeOut);
}


/************************************************************************
 *									*
 * binary methods without access to return header data			*
 *									*
 ************************************************************************/

bool HTTPClient::GETBinary(const std::string &szUrl, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse, const long iTimeOut)
{
	std::vector<std::string> vHeaderData;
	return RESTClient::ExecuteBinary(connection::HTTP::method::GET, szUrl, "", vExtraHeaders, vResponse, vHeaderData, true, iTimeOut);
}


bool HTTPClient::GETBinarySingleLine(const std::string &szUrl, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse, const long iTimeOut)
{
	std::vector<std::string> vHeaderData;
	return RESTClient::ExecuteBinary(connection::HTTP::method::GETSINGLELINE, szUrl, "", vExtraHeaders, vResponse, vHeaderData, true, iTimeOut);
}

bool HTTPClient::POSTBinary(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse, const bool bFollowRedirect, const long iTimeOut)
{
	std::vector<std::string> vHeaderData;
	return RESTClient::ExecuteBinary(connection::HTTP::method::POST, szUrl, szPostdata, vExtraHeaders, vResponse, vHeaderData, true, iTimeOut);
}

bool HTTPClient::PUTBinary(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse, const long iTimeOut)
{
	std::vector<std::string> vHeaderData;
	return RESTClient::ExecuteBinary(connection::HTTP::method::PUT, szUrl, szPutdata, vExtraHeaders, vResponse, vHeaderData, true, iTimeOut);
}

bool HTTPClient::DELETEBinary(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::vector<unsigned char> &vResponse, const long iTimeOut)
{
	std::vector<std::string> vHeaderData;
	return RESTClient::ExecuteBinary(connection::HTTP::method::DELETE, szUrl, szPutdata, vExtraHeaders, vResponse, vHeaderData, true, iTimeOut);
}


/************************************************************************
 *									*
 * methods with access to the return headers				*
 *									*
 ************************************************************************/

bool HTTPClient::GET(const std::string &szUrl, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bIgnoreNoDataReturned)
{
	return RESTClient::Execute((connection::HTTP::method::value)(connection::HTTP::method::GET | connection::HTTP::method::HEAD), szUrl, "", vExtraHeaders, szResponse, vHeaderData, true, -1, bIgnoreNoDataReturned);
}

bool HTTPClient::POST(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bFollowRedirect, const bool bIgnoreNoDataReturned)
{
	return RESTClient::Execute((connection::HTTP::method::value)(connection::HTTP::method::POST | connection::HTTP::method::HEAD), szUrl, szPostdata, vExtraHeaders, szResponse, vHeaderData, bFollowRedirect, -1, bIgnoreNoDataReturned);
}

bool HTTPClient::PUT(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bIgnoreNoDataReturned)
{
	return RESTClient::Execute((connection::HTTP::method::value)(connection::HTTP::method::PUT | connection::HTTP::method::HEAD), szUrl, szPutdata, vExtraHeaders, szResponse, vHeaderData, true, -1, bIgnoreNoDataReturned);
}

bool HTTPClient::DELETE(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bIgnoreNoDataReturned)
{
	return RESTClient::Execute((connection::HTTP::method::value)(connection::HTTP::method::DELETE | connection::HTTP::method::HEAD), szUrl, szPutdata, vExtraHeaders, szResponse, vHeaderData, true, -1, bIgnoreNoDataReturned);
}


/************************************************************************
 *									*
 * methods with optional headers					*
 *									*
 ************************************************************************/

bool HTTPClient::GET(const std::string &szUrl, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const bool bIgnoreNoDataReturned)
{
	std::vector<std::string> vHeaderData;
	return RESTClient::Execute(connection::HTTP::method::GET, szUrl, "", vExtraHeaders, szResponse, vHeaderData, true, -1, bIgnoreNoDataReturned);
}

bool HTTPClient::POST(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const bool bFollowRedirect, const bool bIgnoreNoDataReturned)
{
	std::vector<std::string> vHeaderData;
	return RESTClient::Execute(connection::HTTP::method::POST, szUrl, szPostdata, vExtraHeaders, szResponse, vHeaderData, bFollowRedirect, -1, bIgnoreNoDataReturned);
}

bool HTTPClient::PUT(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const bool bIgnoreNoDataReturned)
{
	std::vector<std::string> vHeaderData;
	return RESTClient::Execute(connection::HTTP::method::PUT, szUrl, szPutdata, vExtraHeaders, szResponse, vHeaderData, true, -1, bIgnoreNoDataReturned);
}

bool HTTPClient::DELETE(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const bool bIgnoreNoDataReturned)
{
	std::vector<std::string> vHeaderData;
	return RESTClient::Execute(connection::HTTP::method::DELETE, szUrl, szPutdata, vExtraHeaders, szResponse, vHeaderData, true, -1, bIgnoreNoDataReturned);
}


/************************************************************************
 *									*
 * simple access methods						*
 *									*
 ************************************************************************/

bool HTTPClient::GET(const std::string &szUrl, std::string &szResponse, const bool bIgnoreNoDataReturned)
{
	std::vector<std::string> vHeaderData;
	std::vector<std::string> vExtraHeaders;
	return RESTClient::Execute(connection::HTTP::method::GET, szUrl, "", vExtraHeaders, szResponse, vHeaderData, true, -1, bIgnoreNoDataReturned);
}

bool HTTPClient::GETSingleLine(const std::string &szUrl, std::string &szResponse, const bool bIgnoreNoDataReturned)
{
	std::vector<std::string> vHeaderData;
	std::vector<std::string> vExtraHeaders;
	return RESTClient::Execute(connection::HTTP::method::GETSINGLELINE, szUrl, "", vExtraHeaders, szResponse, vHeaderData, true, -1, bIgnoreNoDataReturned);
}

bool HTTPClient::GETBinaryToFile(const std::string &szUrl, const std::string &szOutputFile)
{
	std::vector<std::string> vHeaderData;
	std::vector<std::string> vExtraHeaders;
	std::vector<unsigned char> vResponse;
	vResponse.insert(vResponse.begin(), szOutputFile.begin(), szOutputFile.end());
	return RESTClient::ExecuteBinary(connection::HTTP::method::DOWNLOAD, szUrl, "", vExtraHeaders, vResponse, vHeaderData, true, -1);
}


/************************************************************************
 *									*
 * can't inherit static public methods					*
 *									*
 ************************************************************************/

void HTTPClient::SetConnectionTimeout(const long timeout)
{
	RESTClient::SetConnectionTimeout(timeout);
}
void HTTPClient::SetTimeout(const long timeout)
{
	RESTClient::SetTimeout(timeout);
}
void HTTPClient::SetUserAgent(const std::string &useragent)
{
	RESTClient::SetUserAgent(useragent);
}
void HTTPClient::SetSecurityOptions(const bool verifypeer, const bool verifyhost)
{
	RESTClient::SetSecurityOptions(verifypeer, verifyhost);
}
void HTTPClient::SetCookieFile(const std::string &cookiefile)
{
	RESTClient::SetCookieFile(cookiefile);
}

void HTTPClient::Cleanup()
{
	RESTClient::Cleanup();
}


