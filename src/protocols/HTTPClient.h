/*
 * Copyright (c) 2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */
#pragma once
#include "RESTClient.hpp"

class HTTPClient
{
	// give MainWorker acces to the protected Cleanup() function
	friend class MainWorker;

public:

	/************************************************************************
	 *									*
	 * simple access methods						*
	 *   - use for unprotected sites					*
	 *									*
	 ************************************************************************/

	static bool GET(const std::string &szUrl, std::string &szResponse, const bool bIgnoreNoDataReturned = false);
	static bool GETSingleLine(const std::string &szUrl, std::string &szResponse, const bool bIgnoreNoDataReturned = false);
	static bool GETBinaryToFile(const std::string &szUrl, const std::string &szOutputFile);

	/************************************************************************
	 *									*
	 * methods with optional headers					*
	 *   - use for sites that require additional header data		*
	 *     e.g. authorization token, charset definition, etc.		*
	 *									*
	 ************************************************************************/

	static bool GET(const std::string &szUrl, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const bool bIgnoreNoDataReturned = false);
	static bool POST(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const bool bFollowRedirect = true, const bool bIgnoreNoDataReturned = false);
	static bool PUT(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const bool bIgnoreNoDataReturned = false);
	static bool DELETE(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &vExtraHeaders, std::string &szResponse, const bool bIgnoreNoDataReturned = false);


	/************************************************************************
	 *									*
	 * methods with access to the return headers				*
	 *   - use if you require access to the HTTP return codes for		*
	 *     handling specific errors						*
	 *									*
	 ************************************************************************/

	static bool GET(const std::string &szUrl, const std::vector<std::string> &ExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bIgnoreNoDataReturned = false);
	static bool POST(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &ExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bFollowRedirect = true, const bool bIgnoreNoDataReturned = false);
	static bool PUT(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &ExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bIgnoreNoDataReturned = false);
	static bool DELETE(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &ExtraHeaders, std::string &szResponse, std::vector<std::string> &vHeaderData, const bool bIgnoreNoDataReturned = false);


	/************************************************************************
	 *									*
	 * binary methods without access to return header data			*
	 *   - use if you require access to the return content even if there	*
	 *     was an error							*
	 *									*
	 ************************************************************************/

	static bool GETBinary(const std::string &szUrl, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse, const long iTimeOut = -1);
	static bool GETBinarySingleLine(const std::string &szUrl, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse, const long iTimeOut = -1);
	static bool POSTBinary(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse, const bool bFollowRedirect = true, const long iTimeOut = -1);
	static bool PUTBinary(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse, const long iTimeOut = -1);
	static bool DELETEBinary(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse, const long iTimeOut = -1);

	/************************************************************************
	 *									*
	 * binary methods with access to return header data			*
	 *									*
	 ************************************************************************/

	static bool GETBinary(const std::string &szUrl, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse, std::vector<std::string> &vHeaderData, const long iTimeOut = -1);
	static bool GETBinarySingleLine(const std::string &szUrl, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse, std::vector<std::string> &vHeaderData, const long iTimeOut = -1);
	static bool POSTBinary(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse, std::vector<std::string> &vHeaderData, const bool bFollowRedirect = true, const long iTimeOut = -1);
	static bool PUTBinary(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse,
std::vector<std::string> &vHeaderData, const long iTimeOut = -1);
	static bool DELETEBinary(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &ExtraHeaders, std::vector<unsigned char> &vResponse,
std::vector<std::string> &vHeaderData, const long iTimeOut = -1);


	/************************************************************************
	 *									*
	 * can't inherit static public methods					*
	 *									*
	 ************************************************************************/

	static void SetConnectionTimeout(const long timeout);
	static void SetTimeout(const long timeout);
	static void SetUserAgent(const std::string &useragent);
	static void SetSecurityOptions(const bool verifypeer, const bool verifyhost);
	static void SetCookieFile(const std::string &cookiefile);


protected:
	//Cleanup function, should be called before application closed
	static void Cleanup();

};


