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
		activeTariff,			// 5
		instantaneousAmpsUsage,		// 5
		instantaneousAmpsDelivery,	// 6
		instantaneousVoltage,		// 7
		activePowerUsage,		// 8
		activePowerDelivery,		// 9
		mBusDeviceType,			//10
		gasTimestampDSMR2,		//11
		gasUsageDSMR2,			//12
		gasUsageDSMR4,			//13
		endOfTelegram			//14
	};

}; // namespace OBIS {
}; // namespace COSEM {

}; // namespace meter
}; // namespace device

