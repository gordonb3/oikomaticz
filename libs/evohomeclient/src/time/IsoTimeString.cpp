/************************************************************************
 *									*
 *	Time functions							*
 *									*
 ************************************************************************/


#include "IsoTimeString.hpp"
#include <cstring>
#include <ctime>


#ifdef _WIN32
#define localtime_r(timep, result) localtime_s(result, timep)
#define gmtime_r(timep, result) gmtime_s(result, timep)
#endif

#ifndef _WIN32
#define sprintf_s(buffer, buffer_size, stringbuffer, ...) (sprintf(buffer, stringbuffer, __VA_ARGS__))
#endif


int IsoTimeString::m_tzoffset = -1;
int IsoTimeString::m_lastDST = -1;




bool IsoTimeString::verify_date(const std::string szDateTime)
{
	if (szDateTime.length() < 10)
		return false;
	std::string szDate = szDateTime.substr(0,10);
	struct tm mtime;
	mtime.tm_isdst = -1;
	mtime.tm_year = atoi(szDateTime.substr(0, 4).c_str()) - 1900;
	mtime.tm_mon = atoi(szDateTime.substr(5, 2).c_str()) - 1;
	mtime.tm_mday = atoi(szDateTime.substr(8, 2).c_str());
	mtime.tm_hour = 12; // midday time - prevent date shift because of DST
	mtime.tm_min = 0;
	mtime.tm_sec = 0;
	time_t ntime = mktime(&mtime);
	if (ntime == -1)
		return false;
	char cDate[12];
	sprintf_s(cDate, 12, "%04d-%02d-%02d", mtime.tm_year+1900, mtime.tm_mon+1, mtime.tm_mday);
	return (szDate == std::string(cDate));
}


bool IsoTimeString::verify_datetime(const std::string szDateTime)
{
	if (szDateTime.length() < 19)
		return false;
	std::string szDate = szDateTime.substr(0,10);
	std::string szTime = szDateTime.substr(11,8);
	struct tm mtime;
	mtime.tm_isdst = -1;
	mtime.tm_year = atoi(szDateTime.substr(0, 4).c_str()) - 1900;
	mtime.tm_mon = atoi(szDateTime.substr(5, 2).c_str()) - 1;
	mtime.tm_mday = atoi(szDateTime.substr(8, 2).c_str());
	mtime.tm_hour = atoi(szDateTime.substr(11, 2).c_str());
	mtime.tm_min = atoi(szDateTime.substr(14, 2).c_str());
	mtime.tm_sec = atoi(szDateTime.substr(17, 2).c_str());
	time_t ntime = mktime(&mtime);
	if (ntime == -1)
		return false;
	char cDate[12];
	sprintf_s(cDate, 12, "%04d-%02d-%02d", mtime.tm_year+1900, mtime.tm_mon+1, mtime.tm_mday);
	char cTime[12];
	sprintf_s(cTime, 12, "%02d:%02d:%02d", mtime.tm_hour, mtime.tm_min, mtime.tm_sec);
	return ( (szDate == std::string(cDate)) && (szTime == std::string(cTime)) );
}


/*
 * Convert a localtime ISO datetime string to UTC
 */
std::string IsoTimeString::local_to_utc(const std::string szLocalTime)
{
	if (szLocalTime.size() <  19)
		return "";
	if (m_tzoffset == -1)
	{
		// calculate timezone offset once
		time_t now = time(0);
		struct tm utime;
		gmtime_r(&now, &utime);
		utime.tm_isdst = -1;
		m_tzoffset = (int)difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(szLocalTime.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(szLocalTime.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(szLocalTime.substr(8, 2).c_str());
	ltime.tm_hour = atoi(szLocalTime.substr(11, 2).c_str());
	ltime.tm_min = atoi(szLocalTime.substr(14, 2).c_str());
	ltime.tm_sec = atoi(szLocalTime.substr(17, 2).c_str()) + m_tzoffset;
	mktime(&ltime);
	if (m_lastDST == -1)
		m_lastDST = ltime.tm_isdst;
	else if ((m_lastDST != ltime.tm_isdst) && (m_lastDST != -1)) // DST changed - must recalculate timezone offset
	{
		ltime.tm_hour -= (ltime.tm_isdst - m_lastDST);
		m_lastDST = ltime.tm_isdst;
		m_tzoffset = -1;
	}
	char cUntil[22];
	sprintf_s(cUntil, 22, "%04d-%02d-%02dT%02d:%02d:%02dZ", ltime.tm_year+1900, ltime.tm_mon+1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	return std::string(cUntil);
}


/*
 * Convert a UTC ISO datetime string to localtime
 */
std::string IsoTimeString::utc_to_local(const std::string szUTCTime)
{
	if (szUTCTime.size() <  19)
		return "";
	if (m_tzoffset == -1)
	{
		// calculate timezone offset once
		time_t now = time(0);
		struct tm utime;
		gmtime_r(&now, &utime);
		utime.tm_isdst = -1;
		m_tzoffset = (int)difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(szUTCTime.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(szUTCTime.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(szUTCTime.substr(8, 2).c_str());
	ltime.tm_hour = atoi(szUTCTime.substr(11, 2).c_str());
	ltime.tm_min = atoi(szUTCTime.substr(14, 2).c_str());
	ltime.tm_sec = atoi(szUTCTime.substr(17, 2).c_str()) - m_tzoffset;
	mktime(&ltime);
	if (m_lastDST == -1)
		m_lastDST = ltime.tm_isdst;
	else if ((m_lastDST != ltime.tm_isdst) && (m_lastDST != -1)) // DST changed - must recalculate timezone offset
	{
		ltime.tm_hour += (ltime.tm_isdst - m_lastDST);
		m_lastDST = ltime.tm_isdst;
		m_tzoffset = -1;
	}
	char cUntil[40];
	sprintf_s(cUntil, 40, "%04d-%02d-%02dT%02d:%02d:%02dA", ltime.tm_year+1900, ltime.tm_mon+1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	return std::string(cUntil);
}


