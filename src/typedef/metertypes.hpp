#pragma once
#include "typedef/common.hpp"

namespace device {
namespace meter {
namespace type {

enum value {
	ENERGY = 0,		//0
	GAS,			//1
	WATER,			//2
	COUNTER,		//3
	ENERGY_GENERATED,	//4
	TIME,			//5
	END
};

#ifdef INCLUDE_TYPEDEF_CODE
// tiny hack that allows us to keep these definitions together in a single file

const char *Description(const int mType)
{
	static const STR_TABLE_SINGLE Table[] =
	{
		{ ENERGY,		"Energy" },
		{ GAS,			"Gas" },
		{ WATER,		"Water" },
		{ COUNTER,		"Counter" },
		{ ENERGY_GENERATED,	"Energy Generated" },
		{ TIME,			"Time" },
		{ 0, NULL, NULL }
	};
	return findTableIDSingle1(Table, mType);
}



#else // INCLUDE_TYPEDEF_CODE
const char *Description(const int mType);
#endif // INCLUDE_TYPEDEF_CODE


}; // namespace type
}; // namespace meter
}; // namespace device

