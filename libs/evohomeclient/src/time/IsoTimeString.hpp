/************************************************************************
 *									*
 *	Time functions							*
 *									*
 ************************************************************************/
#pragma once
#include <string>


class IsoTimeString
{

public:

	static bool verify_date(const std::string szDateTime);


	static bool verify_datetime(const std::string szDateTime);


/*
 * Convert a localtime ISO datetime string to UTC
 */
	static std::string local_to_utc(const std::string szLocalTime);


/*
 * Convert a UTC ISO datetime string to localtime
 */
	static std::string utc_to_local(const std::string szUTCTime);



private:
	static int m_tzoffset;
	static int m_lastDST;


};
