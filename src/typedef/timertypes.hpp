#pragma once
#include "typedef/common.hpp"

namespace device {
namespace ttimer {
namespace command {

enum value {
	ON = 0,
	OFF
};

}; //namespace command

namespace type {

enum value {
	BEFORESUNRISE = 0,
	AFTERSUNRISE,
	ONTIME,
	BEFORESUNSET,
	AFTERSUNSET,
	FIXEDDATETIME,
	DAYSODD,
	DAYSEVEN,
	WEEKSODD,
	WEEKSEVEN,
	MONTHLY,
	MONTHLY_WD,
	YEARLY,
	YEARLY_WD,
	BEFORESUNATSOUTH,
	AFTERSUNATSOUTH,
	BEFORECIVTWSTART,
	AFTERCIVTWSTART,
	BEFORECIVTWEND,
	AFTERCIVTWEND,
	BEFORENAUTTWSTART,
	AFTERNAUTTWSTART,
	BEFORENAUTTWEND,
	AFTERNAUTTWEND,
	BEFOREASTTWSTART,
	AFTERASTTWSTART,
	BEFOREASTTWEND,
	AFTERASTTWEND,
	END
};

#ifdef INCLUDE_TYPEDEF_CODE
// tiny hack that allows us to keep these definitions together in a single file

const char *Description(const int tType)
{
	static const STR_TABLE_SINGLE Table[] =
	{
		{ BEFORESUNRISE,	"Before Sunrise" },
		{ AFTERSUNRISE,		"After Sunrise" },
		{ ONTIME,		"On Time" },
		{ BEFORESUNSET,		"Before Sunset" },
		{ AFTERSUNSET,		"After Sunset" },
		{ FIXEDDATETIME,	"Fixed Date/Time" },
		{ DAYSODD,		"Odd Day Numbers" },
		{ DAYSEVEN,		"Even Day Numbers" },
		{ WEEKSODD,		"Odd Week Numbers" },
		{ WEEKSEVEN,		"Even Week Numbers" },
		{ MONTHLY,		"Monthly" },
		{ MONTHLY_WD,		"Monthly (Weekday)" },
		{ YEARLY,		"Yearly" },
		{ YEARLY_WD,		"Yearly (Weekday)" },
		{ BEFORESUNATSOUTH,	"Before Sun at South" },
		{ AFTERSUNATSOUTH,	"After Sun at South" },
		{ BEFORECIVTWSTART,	"Before Civil Twilight Start" },
		{ AFTERCIVTWSTART,	"After Civil Twilight Start" },
		{ BEFORECIVTWEND,	"Before Civil Twilight End" },
		{ AFTERCIVTWEND,	"After Civil Twilight End" },
		{ BEFORENAUTTWSTART,	"Before Nautical Twilight Start" },
		{ AFTERNAUTTWSTART,	"After Nautical Twilight Start" },
		{ BEFORENAUTTWEND,	"Before Nautical Twilight End" },
		{ AFTERNAUTTWEND,	"After Nautical Twilight End" },
		{ BEFOREASTTWSTART,	"Before Astronomical Twilight Start" },
		{ AFTERASTTWSTART,	"After Astronomical Twilight Start" },
		{ BEFOREASTTWEND,	"Before Astronomical Twilight End" },
		{ AFTERASTTWEND,	"After Astronomical Twilight End" },
		{ 0, NULL, NULL }
	};
	return findTableIDSingle1(Table, tType);
}



#else // INCLUDE_TYPEDEF_CODE
const char *Description(const int tType);
#endif // INCLUDE_TYPEDEF_CODE


}; // namespace type
}; // namespace ttimer
}; // namespace device

