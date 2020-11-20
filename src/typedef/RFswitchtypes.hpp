#pragma once

#define sSwitchTypeX10				0x00
#define sSwitchTypeARC				0x01
#define sSwitchTypeAB400D			0x02
#define sSwitchTypeWaveman			0x03
#define sSwitchTypeEMW200			0x04
#define sSwitchTypeIMPULS			0x05
#define sSwitchTypeRisingSun			0x06
#define sSwitchTypePhilips			0x07
#define sSwitchTypeEnergenie			0x08
#define sSwitchTypeEnergenie5			0x09
#define sSwitchTypeGDR2				0x0A
#define sSwitchTypeAC				0x0B
#define sSwitchTypeHEU				0x0C
#define sSwitchTypeANSLUT			0x0D
#define sSwitchTypeKambrook			0x0E
#define sSwitchTypeKoppla			0x0F
#define sSwitchTypePT2262			0x10
#define sSwitchTypeLightwaveRF			0x11
#define sSwitchTypeEMW100			0x12
#define sSwitchTypeBBSB				0x13
#define sSwitchTypeMDREMOTE			0x14
#define sSwitchTypeRSL				0x15
#define sSwitchTypeLivolo			0x16
#define sSwitchTypeTRC02			0x17
#define sSwitchTypeAoke				0x18
#define sSwitchTypeTRC02_2			0x19
#define sSwitchTypeEurodomest			0x1A
#define sSwitchTypeLivoloAppliance		0x1B
#define sSwitchTypeBlyss			0x1C
#define sSwitchTypeByronSX			0x1D
#define sSwitchTypeByronMP001			0x1E
#define sSwitchTypeSelectPlus			0x1F


typedef struct _tRFSwitchXref
{
	int switchType;
	int pType;
	int sType;
	std::string szType;
} RFSwitchXref;

//	sSwitchType			pType			sType			RFlink name
const RFSwitchXref rfswitches[] =
{
	{ sSwitchTypeX10,		pTypeLighting1,		sTypeX10,		"X10"		},
	{ sSwitchTypeARC,		pTypeLighting1,		sTypeARC,		"Kaku"		},
	{ sSwitchTypeAB400D,		pTypeLighting1,		sTypeAB400D,		"AB400D"	},
	{ sSwitchTypeWaveman,		pTypeLighting1,		sTypeWaveman,		"Waveman"	},
	{ sSwitchTypeEMW200,		pTypeLighting1,		sTypeEMW200,		"EMW200"	},
	{ sSwitchTypeIMPULS,		pTypeLighting1,		sTypeIMPULS,		"Impuls"	},
	{ sSwitchTypeRisingSun,		pTypeLighting1,		sTypeRisingSun,		"RisingSun"	},
	{ sSwitchTypePhilips,		pTypeLighting1,		sTypePhilips,		"Philips"	},
	{ sSwitchTypeEnergenie,		pTypeLighting1,		sTypeEnergenie,		"Energenie"	},
	{ sSwitchTypeEnergenie5,	pTypeLighting1,		sTypeEnergenie5,	"Energenie5"	},
	{ sSwitchTypeGDR2,		pTypeLighting1,		sTypeGDR2,		"GDR2"		},

	{ sSwitchTypeAC,		pTypeLighting2,		sTypeAC,		"NewKaku"	},
	{ sSwitchTypeHEU,		pTypeLighting2,		sTypeHEU,		"HomeEasy"	},
	{ sSwitchTypeANSLUT,		pTypeLighting2,		sTypeANSLUT,		"Anslut"	},
	{ sSwitchTypeKambrook,		pTypeLighting2,		sTypeKambrook,		"Kambrook"	},

	{ sSwitchTypeKoppla,		pTypeLighting3,		sTypeKoppla,		"Ikea Koppla"	},

	{ sSwitchTypePT2262,		pTypeLighting4,		sTypePT2262,		"PT2262"	},

	{ sSwitchTypeLightwaveRF,	pTypeLighting5,		sTypeLightwaveRF,	"Lightwave"	},
	{ sSwitchTypeEMW100,		pTypeLighting5,		sTypeEMW100,		"EMW100"	},
	{ sSwitchTypeBBSB,		pTypeLighting5,		sTypeBBSB,		"BSB"	`	},
	{ sSwitchTypeMDREMOTE,		pTypeLighting5,		sTypeMDREMOTE,		"MDRemote"	},
	{ sSwitchTypeRSL,		pTypeLighting5,		sTypeRSL,		"Conrad"	},
	{ sSwitchTypeLivolo,		pTypeLighting5,		sTypeLivolo,		"Livolo"	},
	{ sSwitchTypeTRC02,		pTypeLighting5,		sTypeTRC02,		"TRC02RGB"	},
	{ sSwitchTypeAoke,		pTypeLighting5,		sTypeAoke,		"Aoke"		},
	{ sSwitchTypeTRC02_2,		pTypeLighting5,		sTypeTRC02_2,		"TRC022RGB"	},
	{ sSwitchTypeEurodomest,	pTypeLighting5,		sTypeEurodomest,	"Eurodomest"	},
	{ sSwitchTypeLivoloAppliance,	pTypeLighting5,		sTypeLivoloAppliance,	"Livolo App"	},

	{ sSwitchTypeBlyss,		pTypeLighting6,		sTypeBlyss,		"Blyss"		},

	{ sSwitchTypeByronSX,		pTypeChime,		sTypeByronSX,		"Byron"		},
	{ sSwitchTypeByronMP001,	pTypeChime,		sTypeByronMP001,	"Byron MP"	},
	{ sSwitchTypeSelectPlus,	pTypeChime,		sTypeSelectPlus,	"SelectPlus"	},
	{ -1, -1, -1, "" }
};

int GetSwitchTypeFromRFLinkType(const std::string &szType)
{
	int ii = 0;
	while (rfswitches[ii].switchType!=-1)
	{
		if (rfswitches[ii].szType == szType)
			return rfswitches[ii].switchType;
		ii++;
	}
	return -1;
}

std::string GetRFLinkTypeFromSwitchType(const int switchType)
{
	int ii = 0;
	while (rfswitches[ii].switchType!=-1)
	{
		if (rfswitches[ii].switchType == switchType)
			return rfswitches[ii].szType;
		ii++;
	}
	return "";
}

bool GetRFXTypeFromSwitchType(const int& switchType, int& pType, int& sType)
{
	int ii = 0;
	while (rfswitches[ii].gType!=-1)
	{
		if (rfswitches[ii].switchType == switchType)
		{
			pType = rfswitches[ii].pType;
			sType = rfswitches[ii].sType;
			return true;
		}
		ii++;
	}
	return false;
}

bool GetSwitchTypeFromRFXType(int& switchType, const int& pType, const int& sType)
{
	int ii = 0;
	while (rfswitches[ii].gType!=-1)
	{
		if ( (rfswitches[ii].pType == pType) && (rfswitches[ii].sType == sType) )
		{
			switchType = rfswitches[ii].switchType;
			return true;
		}
		ii++;
	}
	return false;
}

