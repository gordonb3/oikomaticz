#pragma once
#include "typedef/common.hpp"

namespace device {
namespace tmeter {

namespace type {
	enum value {
		ENERGY = 0,		//0
		GAS,			//1
		WATER,			//2
		COUNTER,		//3
		ENERGY_GENERATED,	//4
		TIME,			//5
		CITYHEAT,		//6
		END
	};

#ifdef INCLUDE_TYPEDEF_CODE
// tiny hack that allows us to keep these definitions together in a single file

	const char *Description(const int mType)
	{
		static const STR_TABLE_SINGLE Table[] =
		{
			{ ENERGY,		"Electric Energy" },
			{ GAS,			"Gas" },
			{ WATER,		"Water" },
			{ COUNTER,		"Counter" },
			{ ENERGY_GENERATED,	"Electric Energy Generated" },
			{ TIME,			"Time" },
			{ CITYHEAT,		"CityHeat Energy" },
			{ 0, nullptr }
		};
		return findTableIDSingle1(Table, mType);
	}

#else // INCLUDE_TYPEDEF_CODE
	const char *Description(const int mType);
#endif // INCLUDE_TYPEDEF_CODE

}; // namespace type

namespace temperature {
namespace unit {
	enum value {
		CELSIUS = 0,		//0
		FAHRENHEIT,		//1
		UNSUPPORTED
	};
}; // namespace unit
}; // namespace temperature

namespace COSEM { // Companion Specification for Energy Metering
namespace OBIS { // Object identification system
	enum type {
		header = 0,			// 0
		version,			// 1
		electricityUsed,		// 2
		electricityDelivered,		// 3
		activeTariff,			// 4
		activeAmpere,			// 5
		instantaneousVoltage,		// 6
		activePowerUsage,		// 7
		activePowerDelivery,		// 8
		mBusDeviceType,			// 9
		gasTimestampDSMR2,		//10
		gasUsageDSMR2,			//11
		gasUsageDSMR4,			//12
		endOfTelegram,			//13
		waterUsage,			//14
		thermalUsage			//15
	};

}; // namespace OBIS {
}; // namespace COSEM {

}; // namespace tmeter
}; // namespace device

