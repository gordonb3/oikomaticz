#pragma once
#include "typedef/common.hpp"

namespace hardware {
namespace type {

enum value {
	RFXtrx315 = 0,			//0
	RFXtrx433,			//1
	RFXLAN,				//2
	Domoticz,			//3
	P1SmartMeter,			//4
	P1SmartMeterLAN,		//5
	YouLess,			//6
	TE923,				//7
	Rego6XX,			//8
	TTN_MQTT,			//9
	DavisVantage,			//10
	VoltcraftCO20,			//11
	OneWire,			//12
	RaspberryBMP085,		//13
	Wunderground,			//14
	Dummy,				//15
	PiFace,				//16
	S0SmartMeterUSB,		//17
	OpenThermGateway,		//18
	TeleinfoMeter,			//19
	OpenThermGatewayTCP,		//20
	OpenZWave,			//21
	LimitlessLights,		//22
	System,				//23
	EnOceanESP2,			//24
	DarkSky,			//25
	SolarEdgeTCP,			//26
	SBFSpot,			//27
	ICYTHERMOSTAT,			//28
	WOL,				//29
	PVOUTPUT_INPUT,			//30
	EnOceanESP3,			//31
	RaspberryGPIO,			//32
	Meteostick,			//33
	TOONTHERMOSTAT,			//34
	ECODEVICES,			//35
	HarmonyHub,			//36
	Mochad,				//37
	Philips_Hue,			//38
	EVOHOME_SERIAL,			//39
	EVOHOME_SCRIPT,			//40
	MySensorsUSB,			//41
	MySensorsTCP,			//42
	MQTT,				//43
	FRITZBOX,			//44
	ETH8020,			//45
	RFLINKUSB,			//46
	KMTronicUSB,			//47
	KMTronicTCP,			//48
	SOLARMAXTCP,			//49
	KMTronic433,			//50
	Pinger,				//51
	NestThermostat,			//52
	THERMOSMART,			//53
	Netatmo,			//54
	Kodi,				//55
	ANNATHERMOSTAT,			//56
	WINDDELEN,			//57
	SatelIntegra,			//58
	Tellstick,			//59
	LogitechMediaServer,		//60
	RFXtrx868,			//61
	RFLINKTCP,			//62
	Comm5TCP,			//63
	SolarEdgeAPI,			//64
	CurrentCostMeter,		//65
	CurrentCostMeterLAN,		//66
	DomoticzInternal,		//67
	NefitEastLAN,			//68
	PanasonicTV,			//69
	OpenWebNetTCP,			//70
	RaspberryHTU21D,		//71
	AtagOne,			//72
	Sterbox,			//73
	HTTPPOLLER,			//74
	EVOHOME_WEB,			//75
	RAVEn,	    			//76
	S0SmartMeterTCP,		//77
	BuienRadar,			//78
	AccuWeather,			//79
	Comm5Serial,			//80
	Ec3kMeterTCP,			//81
	BleBox,          		//82
	OpenWeatherMap,  		//83
	AlfenEveCharger,		//84
	RaspberryTSL2561,		//85
	Daikin,				//86
	HEOS,				//87
	MultiFun,			//88
	ZIBLUEUSB,			//89
	ZIBLUETCP,			//90
	Yeelight,			//91
	MySensorsMQTT,			//92
	RaspberryPCF8574,		//93
	PythonPlugin,			//94
	XiaomiGateway,			//95
	RaspberryBME280,		//96
	Arilux,				//97
	OpenWebNetUSB,			//98
	IntergasInComfortLAN2RF,	//99
	RelayNet,			//100
	KMTronicUDP,			//101
	SysfsGpio,			//102
	Rtl433,				//103
	OnkyoAVTCP,			//104
	EneverPriceFeeds,		//105
	EVOHOME_TCP,			//106
	USBtinGateway,			//107
	EnphaseAPI,			//108
	RaspberryMCP23017,		//109
	eHouseTCP,			//110
	Comm5SMTCP,			//111
	Nest_OAuthAPI,			//112
	EcoCompteur,			//113
	HoneywellLyric,			//114
	Tado,				//115
	DenkoviHTTPDevices,		//116
	DenkoviUSBDevices,		//117
	DenkoviTCPDevices,		//118
	OctoPrint,			//119
	Tesla,		                //120
	Meteorologisk,	                //121
	Mercedes,			//122
	AirconWithMe,			//123
	TeleinfoMeterTCP,		//124
	MQTTAutoDiscovery,		//125
	RFLINKMQTT,			//126
	VisualCrossing,			//127
	MitsubishiWF,			//128
	LocalTuya = 501,		//501
	END
};

#ifdef INCLUDE_TYPEDEF_CODE
// tiny hack that allows us to keep these definitions together in a single file

/*
 *	  hardware type			long description						short description
 */
const STR_TABLE_SINGLE Descriptions[] =
{
	{ RFXtrx315,			"RFXCOM - RFXtrx315 USB 315MHz Transceiver",			"RFXCOM"		},
	{ RFXtrx433,			"RFXCOM - RFXtrx433 USB 433.92MHz Transceiver",			"RFXCOM"		},
	{ RFXLAN,			"RFXCOM - RFXtrx shared over LAN interface",			"RFXCOM"		},
	{ Domoticz,			"Domoticz - Remote Server",					"Domoticz Remote"	},
	{ DomoticzInternal,		"Domoticz Internal interface",					"Domoticz"		},
	{ P1SmartMeter,			"P1 Smart Meter USB",						"P1 Meter"		},
	{ P1SmartMeterLAN,		"P1 Smart Meter with LAN interface",				"P1 Meter"		},
	{ YouLess,			"YouLess Meter with LAN interface",				"YouLess"		},
	{ WINDDELEN,			"Winddelen",							"Winddelen"		},
	{ TE923,			"TE923 USB Compatible Weather Station",				"TE923"			},
	{ Rego6XX,			"Rego 6XX USB/serial interface",				"Rego_6XX"		},
	{ TTN_MQTT,			"The Things Network (MQTT/CayenneLPP) with LAN interface",	"TTN_MQTT"		},
	{ DavisVantage,			"Davis Vantage Weather Station USB",				"Davis"			},
	{ VoltcraftCO20,		"Voltcraft CO-20 USB air quality sensor",			"Voltcraft"		},
	{ OneWire,			"1-Wire (System)",						"1-Wire"		},
	{ Wunderground,			"Weather Underground",						"WU"			},
	{ DarkSky,			"DarkSky (Weather Lookup)",					"Darksky"		},
	{ VisualCrossing,		"Visual Crossing (Weather Lookup)",				"Visual Crossing"	},
	{ Dummy,			"Dummy (Does nothing, use for virtual switches only)",		"Dummy"			},
	{ Tellstick,			"Tellstick",							"Tellstick"		},
	{ PiFace,			"PiFace - Raspberry Pi IO expansion board",			"PiFace"		},
	{ S0SmartMeterUSB,		"S0 Meter USB",							"S0 Meter"		},
	{ S0SmartMeterTCP,		"S0 Meter with LAN interface",					"S0 Meter"		},
	{ OpenThermGateway,		"OpenTherm Gateway USB",					"OpenTherm"		},
	{ TeleinfoMeter,		"Teleinfo EDF",							"TeleInfo"		},
	{ OpenThermGatewayTCP,		"OpenTherm Gateway with LAN interface",				"OpenTherm"		},
	{ OpenZWave,			"OpenZWave USB",						"OpenZWave"		},
	{ EneverPriceFeeds,		"Enever Dutch Electricity/Gas Price Feed",			"Enever"		},
	{ LimitlessLights,		"Limitless/AppLamp/Mi Light with LAN/WiFi interface",		"Limitless"		},
	{ System,			"Motherboard sensors",						"HardwareMonitor"	},
	{ EnOceanESP2,			"EnOcean USB (ESP2)",						"EnOcean"		},
	{ SolarEdgeTCP,			"SolarEdge via LAN interface",					"SolarEdge"		},
	{ SBFSpot,			"SBFSpot (SMA)",						"SBFSpot"		},
	{ ICYTHERMOSTAT,		"ICY Thermostat",						"ICY"			},
	{ WOL,				"Wake-on-LAN",							"WOL"			},
	{ PVOUTPUT_INPUT,		"PVOutput (Input)",						"PVOutput"		},
	{ EnOceanESP3,			"EnOcean USB (ESP3)",						"EnOcean"		},
	{ Meteostick,			"Meteostick USB",						"Meteostick"		},
	{ TOONTHERMOSTAT,		"Toon Thermostat",						"Toon"			},
	{ ECODEVICES,			"Eco Devices",							"Eco"			},
	{ HarmonyHub,			"Logitech Harmony Hub",						"HarmonyHub"		},
	{ Mochad,			"Mochad CM15Pro/CM19A bridge with LAN interface",		"Mochad"		},
	{ Philips_Hue,			"Philips Hue Bridge",						"Philips Hue"		},
	{ EVOHOME_SERIAL,		"Evohome USB (for HGI/S80)",					"EvoHome"		},
	{ EVOHOME_SCRIPT,		"Evohome via script",						"EvoHome"		},
	{ EVOHOME_WEB,			"Evohome via Web API",						"EvoHome"		},
	{ EVOHOME_TCP,			"Evohome via LAN (remote HGI/S80)",				"EvoHome"		},
	{ MySensorsUSB,			"MySensors Gateway USB",					"MySensors"		},
	{ MySensorsTCP,			"MySensors Gateway with LAN interface",				"MySensors"		},
	{ MySensorsMQTT,		"MySensors Gateway with MQTT interface",			"MySensors"		},
	{ MQTT,				"MQTT Client Gateway with LAN interface",			"MQTT"			},
	{ FRITZBOX,			"Fritzbox Call/Statistics monitor via LAN interface",		"Fritzbox"		},
	{ ETH8020,			"ETH8020 Relay board with LAN interface",			"Eth8020"		},
	{ RFLINKUSB,			"RFLink Gateway USB",						"RFLink"		},
	{ KMTronicUSB,			"KMTronic Gateway USB",						"KMTronic"		},
	{ KMTronicTCP,			"KMTronic Gateway with LAN interface",				"KMTronic"		},
	{ KMTronicUDP,			"KMTronic Gateway with LAN/UDP interface",			"KMTronic"		},
	{ KMTronic433,			"KMTronic 433MHz Gateway USB",					"KMTronic"		},
	{ SOLARMAXTCP,			"SolarMax via LAN interface",					"SolarMax"		},
	{ Pinger,			"System Alive Checker (Ping)",					"Pinger"		},
	{ NestThermostat,		"Nest Thermostat/Protect",					"Nest"			},
	{ Nest_OAuthAPI,		"Nest Thermostat/Protect OAuth",				"Nest"			},
	{ THERMOSMART,			"Thermosmart Thermostat",					"ThermoSmart"		},
	{ Netatmo,			"Netatmo",							"Netatmo"		},
	{ Kodi,				"Kodi Media Server",						"Kodi"			},
	{ PanasonicTV,			"PanasonicTV",							"PanasonicTV"		},
	{ ANNATHERMOSTAT,		"Plugwise Anna Thermostat via LAN interface",			"Plugwise"		},
	{ SatelIntegra,			"Satel Integra via LAN interface",				"Satel Inegra"		},
	{ LogitechMediaServer,		"Logitech Media Server",					"LMS"			},
	{ RFXtrx868,			"RFXCOM - RFXtrx868 USB 868MHz Transceiver",			"RFXCom 868"		},
	{ RFLINKTCP,			"RFLink Gateway with LAN interface",				"RFLink"		},
	{ Comm5TCP,			"Comm5 MA-5XXX with LAN interface",				"Comm5"			},
	{ Comm5SMTCP,			"Comm5 SM-XXXX with LAN interface",				"Comm5"			},
	{ Comm5Serial,			"Comm5 MA-4XXX/MI-XXXX Serial/USB interface",			"Comm5"			},
	{ SolarEdgeAPI,			"SolarEdge via Web API",					"SolarEdge"		},
	{ CurrentCostMeter,		"CurrentCost Meter USB",					"CurrentCost"		},
	{ CurrentCostMeterLAN,		"CurrentCost Meter with LAN interface",				"CurrentCost"		},
	{ NefitEastLAN,			"Nefit Easy HTTP server over LAN interface",			"Nefit"			},
	{ OpenWebNetTCP,		"MyHome OpenWebNet with LAN interface",				"MyHome"		},
	{ AtagOne,			"Atag One Thermostat",						"Atag"			},
	{ Sterbox,			"Sterbox v2-3 PLC with LAN interface",				"Sterbox"		},
	{ HTTPPOLLER,			"HTTP/HTTPS poller",						"HTTP(S) Poller"	},
	{ RAVEn,			"Rainforest RAVEn USB",						"Rainforest"		},
	{ AccuWeather,			"AccuWeather (Weather Lookup)",					"AccuWeather"		},
	{ BleBox,			"BleBox devices",						"BleBox"		},
	{ Ec3kMeterTCP,			"Energy Count 3000/ NETBSEM4/ La Crosse RT-10 LAN",		"Ec3kMeter"		},
	{ OpenWeatherMap,		"Open Weather Map",						"OpenWeatherMap"	},
	{ AlfenEveCharger,		"Alfen Eve Charger LAN",					"Alfen"			},
	{ Daikin,			"Daikin Airconditioning with LAN (HTTP) interface",		"Daikin"		},
	{ HEOS,				"HEOS by DENON",						"HEOS"			},
	{ MultiFun,			"MultiFun LAN",							"Multifun"		},
	{ ZIBLUEUSB,			"ZiBlue RFPlayer USB",						"ZiBlue"		},
	{ ZIBLUETCP,			"ZiBlue RFPlayer with LAN interface",				"ZiBlue"		},
	{ Yeelight,			"YeeLight LED",							"YeeLight"		},
	{ PythonPlugin,			"Python Plugin System",						"Python Plugin System"	},
	{ XiaomiGateway,		"Xiaomi Gateway",						"Xiaomi"		},
	{ RaspberryGPIO,		"Raspberry's GPIO port",					"GPIO"			},
	{ RaspberryBME280,		"I2C sensor BME280 Temp+Hum+Baro",				"I2C BME"		},
	{ RaspberryBMP085,		"I2C sensor BMP085/180 Temp+Baro",				"I2C BM"		},
	{ RaspberryHTU21D,		"I2C sensor HTU21D(F)/SI702x Temp+Humidity",			"I2C HTU21D"		},
	{ RaspberryMCP23017,		"I2C sensor GPIO 16bit expander MCP23017",			"I2C GPIO"		},
	{ RaspberryPCF8574,		"I2C sensor PIO 8bit expander PCF8574(A)",			"I2C PCF8574"		},
	{ RaspberryTSL2561,		"I2C sensor TSL2561 Illuminance",				"I2C TSL2561"		},
	{ Arilux,			"Arilux AL-LC0x",						"Arilux"		},
	{ OpenWebNetUSB,		"MyHome OpenWebNet USB",					"MyHome"		},
	{ IntergasInComfortLAN2RF,	"Intergas InComfort LAN2RF Gateway",				"InComfort"		},
	{ RelayNet,			"Relay-Net 8 channel LAN Relay and binary Input module",	"Relay-Net"		},
	{ SysfsGpio,			"Generic sysfs GPIO",						"sysfs"			},
	{ Rtl433,			"Rtl433 RTL-SDR receiver",					"RTL433"		},
	{ OnkyoAVTCP,			"Onkyo AV Receiver (LAN)",					"Onkyo AV"		},
	{ USBtinGateway,		"USBtin Can Gateway",						"USBtin"		},
	{ EnphaseAPI,			"Enphase Envoy with LAN (HTTP) interface",			"Enphase"		},
	{ eHouseTCP,			"eHouse UDP+TCP with LAN interface",				"eHouse"		},
	{ EcoCompteur,			"EcoCompteur Legrand with LAN interface",			"EcoCompteur"		},
	{ HoneywellLyric,		"Honeywell Lyric Thermostat",					"Lyric"			},
	{ Tado,				"Tado Thermostat",						"Tado"			},
	{ DenkoviHTTPDevices,		"Denkovi Modules with LAN (HTTP) Interface",			"Denkovi"		},
	{ DenkoviUSBDevices,		"Denkovi Modules with USB Interface",				"Denkovi"		},
	{ DenkoviTCPDevices,		"Denkovi Modules with LAN (TCP) Interface",			"Denkovi"		},
	{ BuienRadar,			"Buienradar (Dutch Weather Information)",			"BuienRadar"		},
	{ OctoPrint,			"OctoPrint (MQTT/Gina Haussge) with LAN interface",		"OctoPrint"		},
	{ Tesla,			"Tesla Model S/3/X",						"Tesla"			},
	{ Meteorologisk,		"Meteorologisk institutt Norway (Weather Lookup)",		"Meteorologisk"		},
	{ Mercedes,			"Mercedes ME Connect",						"Mercedes"		},
	{ AirconWithMe,			"AirconWithMe Wifi Airco module",				"AirconWithMe"		},
	{ TeleinfoMeterTCP,		"Teleinfo EDF with LAN interface",				"TeleInfo"		},
	{ MQTTAutoDiscovery,		"MQTT Auto Discovery Client Gateway with LAN interface",	"MQTT-AD"		},
	{ RFLINKMQTT,			"RFLink Gateway MQTT",						"RFLink"		},
	{ MitsubishiWF,			"Mitsubishi WF RAC Airco with LAN (HTTP) interface",		"MitsubishiWF"		},
	{ LocalTuya,			"Local Tuya provider",						"LocalTuya"		},
	{ 0, nullptr, nullptr }
};


const char *Long_Desc(int hType)
{
	return findTableIDSingle1(Descriptions, hType);
}

const char *Short_Desc(int hType)
{
	return findTableIDSingle2(Descriptions, hType);
}


#else // INCLUDE_TYPEDEF_CODE
const char *Long_Desc(int hType);
const char *Short_Desc(int hType);
#endif // INCLUDE_TYPEDEF_CODE


}; // namespace type
}; // namespace hardware


