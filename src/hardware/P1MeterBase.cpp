#include "stdafx.h"
#include "P1MeterBase.h"
#include "hardware/hardwaretypes.h"
#include "main/SQLHelper.h"
#include "main/localtime_r.h"
#include "main/Logger.h"
#include "main/json_helper.h"
#include "main/mainworker.h"
#include "main/WebServer.h"
#include "typedef/metertypes.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>

#define CRC16_ARC	0x8005
#define CRC16_ARC_REFL	0xA001

#define CUSTOM_IMAGE_ID 19		// row index inside 'switch_icons.txt'
#define OBIS_MAX_VALUE_LENGTH 20	// the maximum number of characters that a value can have

constexpr std::array<uint16_t, 256> p1_crc_16 {
	0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
	0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
	0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
	0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
	0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
	0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
	0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
	0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
	0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
	0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
	0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
	0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
	0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
	0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
	0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
	0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
	0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
	0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
	0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
	0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
	0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
	0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
	0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
	0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
	0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
	0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
	0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
	0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
	0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
	0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
	0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
	0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

P1MeterBase::P1MeterBase()
{
	m_bDisableCRC = false;
	m_ratelimit = 0;
	m_applylimits = false;
	Init();
}


P1MeterBase::~P1MeterBase()
{
}


void P1MeterBase::Init()
{
	memset(&m_power, 0, sizeof(m_power));
	m_power.len = sizeof(P1Power) - 1;
	m_power.type = pTypeP1Power;
	m_power.subtype = sTypeP1Power;
	m_power.ID = sTypeP1Power;

	memset(&m_gas, 0, sizeof(m_gas));
	m_gas.len = sizeof(P1BusDevice) - 1;
	m_gas.type = pTypeP1BusDevice;
	m_gas.subtype = sTypeP1Gas;
	m_gas.ID = sTypeP1Gas;

	memset(&m_water, 0, sizeof(m_water));
	m_water.len = sizeof(P1BusDevice) - 1;
	m_water.type = pTypeP1BusDevice;
	m_water.subtype = sTypeP1Water;
	m_water.ID = sTypeP1Water;

	memset(&m_thermal, 0, sizeof(m_thermal));
	m_thermal.len = sizeof(P1BusDevice) - 1;
	m_thermal.type = pTypeP1BusDevice;
	m_thermal.subtype = sTypeP1CityHeat;
	m_thermal.ID = sTypeP1CityHeat;

	memset(&m_phasedata, 0, sizeof(m_phasedata));

	memset(&m_buffer, 0, sizeof(m_buffer));
	m_bufferpos = 0;
	m_exclmarkfound = 0;

	memset(&m_lbuffer, 0, sizeof(m_lbuffer));
	m_lbufferpos = 0;
	m_lexclmarkfound = 0;

	m_p1version = 0;
	m_phasecount = 1;
	m_linecount = 0;
	m_CRfound = 0;
	m_lastUpdateTime = 0;
	m_lastTariff = 0;

	memset(&m_privgas, 0, sizeof(m_privgas));
	memset(&m_privwater, 0, sizeof(m_privwater));
	memset(&m_privthermal, 0, sizeof(m_privthermal));

	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT nValue FROM DeviceStatus WHERE (HardwareID=%d) AND (Type=%d) AND (SubType=%d) AND (DeviceID='000000FF')",
		m_HwdID, pTypeGeneralSwitch, sSwitchGeneralSwitch);
	if (!result.empty())
	{
		m_lastTariff = 2 - static_cast<float>(stod(result[0][0]));
	}

	m_isencrypteddata = false;
	m_mbusonly = true;
}

// returns false if an error is detected
bool P1MeterBase::MatchLine()
{
	if ((strlen((const char*)&m_lbuffer) < 1) || (m_lbuffer[0] == 0x0a))
		return true; // null value (startup)

	device::tmeter::COSEM::OBIS::type matchtype;
	std::string vString;
	char value[(OBIS_MAX_VALUE_LENGTH + 1)] = "";
	unsigned char phase = 0; // L1..L3
	unsigned char tariff_id = 0;


	// STEP 1: match the OBIS codes that we want to process
	switch (m_lbuffer[0])
	{
	case '1':
		if (m_lbuffer[2] == '0')
		{
			// electricity meter
			if (m_lbuffer[5] == '.')
			{
				// Try to match OBIS IDs (n = tariff id):
				//    electricity used (all phases):		1-0:1.8.n
				//    electricity delivered (all phases):	1-0:2.8.n
				//    active power usage (all phases):		1-0:1.7.0
				//    active power delivery (all phases):	1-0:2.7.0

				// FIXME
				// Nordic meters also keep track of reactive power for billing
				//    reactive electricity used (all phases):		1-0:3.8.n
				//    reactive electricity delivered (all phases):	1-0:4.8.n
				//    reactive power usage (all phases):		1-0:3.7.0
				//    reactive power delivery (all phases):		1-0:4.7.0

				if (m_lbuffer[4] == '1')
				{
					// electric power usage totals
					if ((m_lbuffer[6] == '7') && (m_lbuffer[8] == '0'))
						matchtype = device::tmeter::COSEM::OBIS::activePowerUsage;
					else if (m_lbuffer[6] == '8')
						matchtype = device::tmeter::COSEM::OBIS::electricityUsed;
					else
						return true;
				}
				else if (m_lbuffer[4] == '2')
				{
					// electric power delivery totals
					if ((m_lbuffer[6] == '7') && (m_lbuffer[8] == '0'))
						matchtype = device::tmeter::COSEM::OBIS::activePowerDelivery;
					else if (m_lbuffer[6] == '8')
						matchtype = device::tmeter::COSEM::OBIS::electricityDelivered;
					else
						return true;
				}
				else
					return true;
			}
			else if ((m_lbuffer[7] != '7') || (m_lbuffer[9] != '0'))
				return true;
			else if (m_lbuffer[5] == '1')
			{
				// Try to match OBIS IDs:
				//    active power usage L1-L3:		1-0:21.7.0	1-0:41.7.0	1-0:61.7.0
				//    active current L1-L3:		1-0:31.7.0	1-0:51.7.0	1-0:71.7.0

				if (m_lbuffer[4] & 0x1)
					if (m_phasedata.ampere[0] < 0)
						return true;	 // active current has a 1 Ampere resolution which is too rough for our purpose
					else
						matchtype = device::tmeter::COSEM::OBIS::activeAmpere;
				else if (m_phasecount > 0)
					matchtype = device::tmeter::COSEM::OBIS::activePowerUsage;
				else
					return true;
			}
			else if (m_lbuffer[5] == '2')
			{
				// Try to match OBIS IDs:
				//    active power delivery L1-L3:	1-0:22.7.0	1-0:42.7.0	1-0:62.7.0
				//    instantaneous voltage L1-L3:	1-0:32.7.0	1-0:52.7.0	1-0:72.7.0

				if (m_lbuffer[4] & 0x1)
					matchtype = device::tmeter::COSEM::OBIS::instantaneousVoltage;	// instantaneous voltage
				else if (m_phasecount > 0)
					matchtype = device::tmeter::COSEM::OBIS::activePowerDelivery;	// instantaneous power delivery
				else
					return true;
			}
			else
				// FIXME
				// Nordic meters also keep track of reactive power for billing
				//    reactive power usage L1-L3:	1-0:23.7.0	1-0:43.7.0	1-0:63.7.0
				//    reactive power delivery L1-L3:	1-0:24.7.0	1-0:44.7.0	1-0:64.7.0

				return true;

		}
		else if ((m_p1version == 0) && (m_lbuffer[2] == '3'))	// get meter version only once
		{
			// Try to match OBIS ID:
			//    P1 version:		1-3:0.2.8

			if (strncmp("0.2.8",(const char*)&m_lbuffer + 4,5) == 0)
				matchtype = device::tmeter::COSEM::OBIS::version;
			else
				return true;
		}
		else
			return true;
		break;

	case '0':
		// M-bus
		if (m_lbuffer[2] == '0')
		{
			// Try to match OBIS IDs:
			//    timestamp:		0-0:1.0.0	-- possible future use
			//    version P1+eMucs:		0-0:96.1.4
			//    tariff indicator:		0-0:96.14.0
			if (strncmp("96.14.0",(const char*)&m_lbuffer + 4,7) == 0)
				matchtype = device::tmeter::COSEM::OBIS::activeTariff;
			else if ((m_p1version == 0) && (strncmp("96.1.4",(const char*)&m_lbuffer + 4,6) == 0))	// get meter version only once
			{
				matchtype = device::tmeter::COSEM::OBIS::version;
			}
			else
				return true;
		}
		else if (m_lbuffer[7] == '1')
		{
			// Try to match OBIS ID (n = m-bus channel):
			//    device type:		0-n:24.1.0

			if (m_lbuffer[2] == m_privgas.channel)	// get gas meter channel only once
				return true;
			if (m_lbuffer[2] == m_privwater.channel)
				return true;
			if (m_lbuffer[2] == m_privthermal.channel)
				return true;

			if (strncmp("24.1.0",(const char*)&m_lbuffer + 4,6) == 0)
				matchtype = device::tmeter::COSEM::OBIS::mBusDeviceType;
			else
				return true;
		}
		else if (m_lbuffer[2] == m_privgas.channel)
		{
			// Try to match OBIS IDs (n = m-bus channel):
			//    DSMR4+ gas usage (NLD):				0-n:24.2.1
			//    not temperature corrected gas usage (BEL):	0-n:24.2.3
			//    DSMR2 timestamp:					0-n:24.3.0

			if (strncmp("24.2.",(const char*)&m_lbuffer + 4,5) == 0)
			{
				if ((m_lbuffer[9] & 0xFD) != '1')
					return true;
				matchtype = device::tmeter::COSEM::OBIS::gasUsageDSMR4;
			}
			else if ((m_p1version < 4) && (strncmp("24.3.0",(const char*)&m_lbuffer + 4,6) == 0))
				matchtype = device::tmeter::COSEM::OBIS::gasTimestampDSMR2;
			else
				return true;
		}
		else if (m_lbuffer[2] == m_privwater.channel)
		{
			// Try to match OBIS IDs (n = m-bus channel):
			//    DSMR4+ utility usage:				0-n:24.2.1

			if (strncmp("24.2.1",(const char*)&m_lbuffer + 4,6) == 0)
			{
				matchtype = device::tmeter::COSEM::OBIS::waterUsage;
			}
			else
				return true;
		}
		else if (m_lbuffer[2] == m_privthermal.channel)
		{
			// Try to match OBIS IDs (n = m-bus channel):
			//    DSMR4+ utility usage:				0-n:24.2.1

			if (strncmp("24.2.1",(const char*)&m_lbuffer + 4,6) == 0)
			{
				matchtype = device::tmeter::COSEM::OBIS::thermalUsage;
			}
			else
				return true;
		}
		else
			return true;
		break;

	case '/':
		// header - start of telegram.
		// matchtype = device::tmeter::COSEM::OBIS::SMID;
		m_linecount = 1;
		return true; // we do not process anything else on this line
		break;

	case '!':
		// end of telegram
		matchtype = device::tmeter::COSEM::OBIS::endOfTelegram;
		break;

	case '(':
		// gas usage DSMR v2
		if (m_linecount == 18)
			matchtype = device::tmeter::COSEM::OBIS::gasUsageDSMR2;
		else
			return true;
		break;

	default:
		return true;
		break;
	}

	// STEP 2: if our telegram is complete, upload the data to mainworker
	if (matchtype == device::tmeter::COSEM::OBIS::endOfTelegram)
	{
		m_lexclmarkfound = 1;
		m_lastUpdateTime = m_receivetime;

		if (m_p1version == 0) // meter did not report its DSMR version
		{
			Log(LOG_STATUS, "Meter does not report its version - using DSMR 2.2 compatibility");
			m_p1version = 2;
		}

		if (!m_mbusonly)	// only send electricity data if the attached meter actually registers it
		{
			if (m_phasecount == 1)		// single phase meter: L1 power is the same as total power
				m_phasecount = 0;	// disable further processing of individual phase instantaneous power entries

			sDecodeRXMessage(this, (const unsigned char *)&m_power, "Power", 255, nullptr);

			if ((m_currentTariff != m_lastTariff) && (m_power.powerusage2 > 0))
			{
				m_lastTariff = m_currentTariff;
				int sstate = (m_currentTariff == 1) ? gswitch_sOn : gswitch_sOff;
				UpsertSwitch(255, device::tswitch::type::OnOff, sstate, "Power Tariff Low");
			}

			if (m_phasedata.voltage[0])
			{
				std::string defaultname = "Voltage L1";
				for (int i=0; i<=m_phasecount; )
				{
					i++;
					defaultname[9] = i | 0x30;
					SendVoltageSensor(0, i, 255, m_phasedata.voltage[i], defaultname);
				}
			}

			if (m_phasedata.ampere[0] > 0)
			{
				SendCurrentSensor(0, 255, m_phasedata.ampere[1], m_phasedata.ampere[2], m_phasedata.ampere[3], "Current");
			}

			if (m_phasedata.instpwruse[0] && (m_phasecount > 0))
			{
				std::string defaultname = "Usage L1";
				for (int i=1; i<=m_phasecount; i++)
				{
					defaultname[7] = i | 0x30;
					SendWattMeter(0, i, 255, m_phasedata.instpwruse[i], defaultname);
				}
			}

			if (m_phasedata.instpwrdel[0] && (m_phasecount > 0))
			{
				std::string defaultname = "Delivery L1";
				for (int i=1; i<=m_phasecount; i++)
				{
					defaultname[10] = i | 0x30;
					SendWattMeter(0, i+3, 255, m_phasedata.instpwrdel[i], defaultname);
				}
			}
		}

		// M-Bus data is only updated once every 5 minutes but this appears to be relative to when the meter was booted
		// to track when this happens we only update our logs when the value changes and we use the datetime value that
		// is supplied by the meter itself as a reference rather than our own time.

		if ((m_gas.usage > 0) && ((m_gas.usage != m_privgas.lastusage) || (difftime(m_receivetime, m_privgas.lastsubmittime) >= 300)))
		{
			// only update gas when there is a new value, or 5 minutes are passed
			if (m_privgas.clockskew >= 300)
			{
				// just accept it - we cannot sync to our clock
				m_privgas.lastsubmittime = m_receivetime;
				m_privgas.lastusage = m_gas.usage;
				sDecodeRXMessage(this, (const unsigned char *)&m_gas, "Gas", 255, nullptr);
			}
			else if (m_receivetime >= m_privgas.nextmetertime)
			{
				struct tm ltime;
				localtime_r(&m_receivetime, &ltime);
				char myts[80];
				sprintf(myts, "%02d%02d%02d%02d%02d%02dW", ltime.tm_year % 100, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
				if (ltime.tm_isdst)
					myts[12] = 'S';
				if (strncmp((const char*)&myts, m_privgas.timestring, 13) >= 0)
				{
					m_privgas.lastsubmittime = m_receivetime;
					m_privgas.lastusage = m_gas.usage;
					m_privgas.nextmetertime += 300;
					sDecodeRXMessage(this, (const unsigned char *)&m_gas, "Gas", 255, nullptr);
				}
				else // clock is ahead
				{
					time_t gtime = ParseP1datetime(m_privgas.timestring);
					m_privgas.clockskew = difftime(gtime, m_receivetime);
					if (m_privgas.clockskew >= 300)
					{
						Log(LOG_ERROR, "Unable to synchronize to the gas meter clock because it is more than 5 minutes ahead of my time");
					}
					else {
						m_privgas.nextmetertime = gtime;
						Log(LOG_STATUS, "Gas meter clock is %i seconds ahead - wait for my clock to catch up", (int)m_privgas.clockskew);
					}
				}
			}
		}

		if ((m_water.usage > 0) && ((m_water.usage != m_privwater.lastusage) || (difftime(m_receivetime, m_privwater.lastsubmittime) >= 300)))
		{
			// only update water when there is a new value, or 5 minutes are passed
			if (m_privwater.clockskew >= 300)
			{
				// just accept it - we cannot sync to our clock
				m_privwater.lastsubmittime = m_receivetime;
				m_privwater.lastusage = m_water.usage;
				sDecodeRXMessage(this, (const unsigned char *)&m_water, "Water", 255, nullptr);
			}
			else if (m_receivetime >= m_privwater.nextmetertime)
			{
				struct tm ltime;
				localtime_r(&m_receivetime, &ltime);
				char myts[80];
				sprintf(myts, "%02d%02d%02d%02d%02d%02dW", ltime.tm_year % 100, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
				if (ltime.tm_isdst)
					myts[12] = 'S';
				if (strncmp((const char*)&myts, m_privwater.timestring, 13) >= 0)
				{
					m_privwater.lastsubmittime = m_receivetime;
					m_privwater.lastusage = m_water.usage;
					m_privwater.nextmetertime += 300;
					sDecodeRXMessage(this, (const unsigned char *)&m_water, "Water", 255, nullptr);
				}
				else // clock is ahead
				{
					time_t gtime = ParseP1datetime(m_privwater.timestring);
					m_privwater.clockskew = difftime(gtime, m_receivetime);
					if (m_privwater.clockskew >= 300)
					{
						Log(LOG_ERROR, "Unable to synchronize to the water meter clock because it is more than 5 minutes ahead of my time");
					}
					else {
						m_privwater.nextmetertime = gtime;
						Log(LOG_STATUS, "Water meter clock is %i seconds ahead - wait for my clock to catch up", (int)m_privwater.clockskew);
					}
				}
			}
		}

		if ((m_thermal.usage > 0) && ((m_thermal.usage != m_privthermal.lastusage) || (difftime(m_receivetime, m_privthermal.lastsubmittime) >= 300)))
		{
			// only update thermal when there is a new value, or 5 minutes are passed
			if (m_privthermal.clockskew >= 300)
			{
				// just accept it - we cannot sync to our clock
				m_privthermal.lastsubmittime = m_receivetime;
				m_privthermal.lastusage = m_thermal.usage;
				sDecodeRXMessage(this, (const unsigned char *)&m_thermal, "City Heat", 255, nullptr);
			}
			else if (m_receivetime >= m_privthermal.nextmetertime)
			{
				struct tm ltime;
				localtime_r(&m_receivetime, &ltime);
				char myts[80];
				sprintf(myts, "%02d%02d%02d%02d%02d%02dW", ltime.tm_year % 100, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
				if (ltime.tm_isdst)
					myts[12] = 'S';
				if (strncmp((const char*)&myts, m_privthermal.timestring, 13) >= 0)
				{
					m_privthermal.lastsubmittime = m_receivetime;
					m_privthermal.lastusage = m_thermal.usage;
					m_privthermal.nextmetertime += 300;
					sDecodeRXMessage(this, (const unsigned char *)&m_thermal, "Gas", 255, nullptr);
				}
				else // clock is ahead
				{
					time_t gtime = ParseP1datetime(m_privthermal.timestring);
					m_privthermal.clockskew = difftime(gtime, m_receivetime);
					if (m_privthermal.clockskew >= 300)
					{
						Log(LOG_ERROR, "Unable to synchronize to the thermal meter clock because it is more than 5 minutes ahead of my time");
					}
					else {
						m_privthermal.nextmetertime = gtime;
						Log(LOG_STATUS, "CityHeat meter clock is %i seconds ahead - wait for my clock to catch up", (int)m_privthermal.clockskew);
					}
				}
			}
		}

		m_linecount = 0;
		m_lexclmarkfound = 0;

		return true;
	}

	// else
	// STEP 3: prepare and validate the data content
	switch (matchtype)
	{
	case device::tmeter::COSEM::OBIS::electricityUsed:
	case device::tmeter::COSEM::OBIS::electricityDelivered:
		m_mbusonly = false;
		if (m_lbuffer[9] != '(')
			return true; // no support for tariff id >= 10
		tariff_id = m_lbuffer[8] ^ 0x30;
		if (tariff_id > 2)
			return true; // currently only supporting tariffs: 0 (LUX: single), 1 (NLD,BEL: low,night), 2 (NLD,BEL: high,day)
		vString = (const char*)&m_lbuffer + 10;
		break;
	case device::tmeter::COSEM::OBIS::activeTariff:
		vString = (const char*)&m_lbuffer + 12;
		break;
	case device::tmeter::COSEM::OBIS::activePowerUsage:
	case device::tmeter::COSEM::OBIS::activePowerDelivery:
	case device::tmeter::COSEM::OBIS::instantaneousVoltage:
	case device::tmeter::COSEM::OBIS::activeAmpere:
		if (m_lbuffer[9] == '(')
			vString = (const char*)&m_lbuffer + 10;
		else
		{
			phase = ((m_lbuffer[4] & 0xFE) ^ 0x30) >> 1;
			if (!phase || (phase > 3)) // phase number out of bounds
				return true;
			if (phase > m_phasecount)
				m_phasecount = phase;
			vString = (const char*)&m_lbuffer + 11;
		}
		break;
	case device::tmeter::COSEM::OBIS::gasUsageDSMR4:
	case device::tmeter::COSEM::OBIS::waterUsage:
	case device::tmeter::COSEM::OBIS::thermalUsage:
		vString = (const char*)&m_lbuffer + 26;
		break;
	case device::tmeter::COSEM::OBIS::gasTimestampDSMR2:
	case device::tmeter::COSEM::OBIS::mBusDeviceType:
		vString = (const char*)&m_lbuffer + 11;
		break;
	case device::tmeter::COSEM::OBIS::version:
		vString = (const char*)&m_lbuffer + 10;
		break;
	case device::tmeter::COSEM::OBIS::gasUsageDSMR2:
		vString = (const char*)&m_lbuffer + 1;
		break;
	}

	// STEP 4: extract the value from the line
	int ePos = vString.find_first_of("*)");

	if (ePos == std::string::npos)
	{
		// invalid message: value not delimited
		Log(LOG_NORM, "Dismiss incoming - value is not delimited in line \"%s\"", m_lbuffer);
		return false;
	}

	if (ePos > OBIS_MAX_VALUE_LENGTH)
	{
		if (matchtype == device::tmeter::COSEM::OBIS::version)
		{
			// Sibelga (Brussels) meters send an invalid version string
			ePos = 6;
		}
		else
		{
			// invalid message: line too long
			Log(LOG_NORM, "Dismiss incoming - value in line \"%s\" is oversized", m_lbuffer);
			return false;
		}
	}

	if (ePos == 0)
	{
		// invalid message: value is empty
		Log(LOG_NORM, "Dismiss incoming - value in line \"%s\" is empty", m_lbuffer);
		return false;
	}

	strcpy(value, vString.substr(0, ePos).c_str());
	char *validate = value + ePos;
	unsigned long temp_usage;

	// STEP 5: convert and store the cleaned data content
	switch (matchtype)
	{
	case device::tmeter::COSEM::OBIS::electricityDelivered:
		temp_usage = (unsigned long)(strtod(value, &validate)*1000.0f);
		if (tariff_id == 2)
		{
			if (!m_power.powerdeliv2 || (m_p1version >= 4) || ((temp_usage - m_power.powerdeliv2) < 10000))
				m_power.powerdeliv2 = temp_usage;
		}
		else
		{
			if (!m_power.powerdeliv1 || (m_p1version >= 4) || ((temp_usage - m_power.powerdeliv1) < 10000))
				m_power.powerdeliv1 = temp_usage;
		}
		break;
	case device::tmeter::COSEM::OBIS::electricityUsed:
		temp_usage = (unsigned long)(strtod(value, &validate)*1000.0f);
		if (tariff_id == 2)
		{
			if (!m_power.powerusage2 || (m_p1version >= 4) || ((temp_usage - m_power.powerusage2) < 10000))
				m_power.powerusage2 = temp_usage;
		}
		else
		{
			if (!m_power.powerusage1 || (m_p1version >= 4) || ((temp_usage - m_power.powerusage1) < 10000))
				m_power.powerusage1 = temp_usage;
		}
	case device::tmeter::COSEM::OBIS::activeTariff:
		m_currentTariff = static_cast<float>(strtod(value, &validate));
		break;
	case device::tmeter::COSEM::OBIS::activePowerDelivery:
		if (phase > 0)
		{
			m_phasedata.instpwrdel[0] = 1;
			float temp_power = static_cast<float>(strtod(value, &validate)*1000.0f);
			if (!m_applylimits || (temp_power < 10000))
				m_phasedata.instpwrdel[phase] = temp_power;
		}
		else
		{
			temp_usage = (unsigned long)(strtod(value, &validate)*1000.0f);
			if (!m_applylimits || (temp_usage < 17250))
				m_power.delivcurrent = temp_usage;
		}
		break;
	case device::tmeter::COSEM::OBIS::activePowerUsage:
		if (phase > 0)
		{
			m_phasedata.instpwruse[0] = 1;
			float temp_power = static_cast<float>(strtod(value, &validate)*1000.0f);
			if (!m_applylimits || (temp_power < 10000))
				m_phasedata.instpwruse[phase] = temp_power;
		}
		else
		{
			temp_usage = (unsigned long)(strtod(value, &validate)*1000.0f);
			if (!m_applylimits || (temp_usage < 17250))
				m_power.usagecurrent = temp_usage;
		}
		break;
	case device::tmeter::COSEM::OBIS::instantaneousVoltage:
		{
			m_phasedata.voltage[0] = 1;
			float temp_volt = strtof(value, &validate);
			if (!m_applylimits || (temp_volt < 300))
				m_phasedata.voltage[phase] = temp_volt;
		}
		break;
	case device::tmeter::COSEM::OBIS::activeAmpere:
		if (m_phasedata.ampere[0] == 0)
		{
			vString = (const char*)&value;
			ePos = vString.find('.');
			if (ePos == std::string::npos)
				m_phasedata.ampere[0] = -1;
			else
				m_phasedata.ampere[0] = 1;
		}
		if (m_phasedata.ampere[0] > 0)
		{
			float temp_ampere = strtof(value, &validate);
			if (!m_applylimits || (temp_ampere < 300))
				m_phasedata.ampere[phase] = temp_ampere;
		}
		break;
	case device::tmeter::COSEM::OBIS::gasUsageDSMR4:
		m_gas.usage = (unsigned long)(strtod(value, &validate)*1000.0f);
		// need to get timestamp from this line as well
		vString = (const char*)&m_lbuffer + 11;
		ePos = vString.find_first_of("*)");
		if ((ePos == std::string::npos) || (ePos > 13))
			return false;
		strcpy(m_privgas.timestring, vString.substr(0, ePos).c_str());
		break;
	case device::tmeter::COSEM::OBIS::waterUsage:
		m_water.usage = (unsigned long)(strtod(value, &validate)*1000.0f);
		// need to get timestamp from this line as well
		vString = (const char*)&m_lbuffer + 11;
		ePos = vString.find_first_of("*)");
		if ((ePos == std::string::npos) || (ePos > 13))
			return false;
		strcpy(m_privwater.timestring, vString.substr(0, ePos).c_str());
		break;
	case device::tmeter::COSEM::OBIS::thermalUsage:
		m_thermal.usage = (unsigned long)(strtod(value, &validate)*1000.0f);
		// need to get timestamp from this line as well
		vString = (const char*)&m_lbuffer + 11;
		ePos = vString.find_first_of("*)");
		if ((ePos == std::string::npos) || (ePos > 13))
			return false;
		strcpy(m_privthermal.timestring, vString.substr(0, ePos).c_str());
		break;
	case device::tmeter::COSEM::OBIS::gasTimestampDSMR2:
		if (ePos > 13)
		{
			Log(LOG_NORM, "Dismiss incoming - invalid gas timestamp value in line \"%s\"", m_lbuffer);
			return false;
		}
		strcpy(m_privgas.timestring, value);
		m_linecount = 17;
		break;
	case device::tmeter::COSEM::OBIS::gasUsageDSMR2:
		temp_usage = (unsigned long)(strtod(value, &validate)*1000.0f);
		if (!m_gas.usage || ((temp_usage - m_gas.usage) < 20000))
			m_gas.usage = temp_usage;
		break;
	case device::tmeter::COSEM::OBIS::mBusDeviceType:
		temp_usage = (unsigned long)(strtod(value, &validate));
		if (temp_usage == 3)
		{
			m_privgas.channel = (char)m_lbuffer[2];
			Log(LOG_STATUS, "Found gas meter on M-Bus channel %c", m_privgas.channel);
		}
		else if (temp_usage == 4)
		{
			m_privthermal.channel = (char)m_lbuffer[2];
			Log(LOG_STATUS, "Found thermal meter on M-Bus channel %c", m_privthermal.channel);
		}
		else if (temp_usage == 7)
		{
			m_privwater.channel = (char)m_lbuffer[2];
			Log(LOG_STATUS, "Found water meter on M-Bus channel %c", m_privwater.channel);
		}
		break;
	case device::tmeter::COSEM::OBIS::version:
		char szVersion[12];
		if ((value[0] == '(') && (ePos == 6))
		{
			// Belgian meter
			if (value[1] == '2')
			{
				// Sibelga meter sends incorrect version string
				Log(LOG_ERROR, "Meter appears to be from Sibelga. Guessing version as 5.x");
				m_p1version = 5;
				break;
			}
			else
			{
				sprintf(szVersion, "ESMR %c.%c.%c", value[1], value[2], value[3]);
				m_p1version = value[1] ^ 0x30;
			}
		}
		else
		{
			// Dutch meter
			sprintf(szVersion, "ESMR %c.%c", value[0], value[1]);
			m_p1version = value[0] ^ 0x30;
			if (m_p1version < 5)
				szVersion[0]='D';
		}
		Log(LOG_STATUS, "Meter reports as %s", szVersion);
		if (m_p1version > 9)
			m_p1version = 0;
		break;
	}

	return true;
}


/*
/ GB3:	DSMR 4.0 defines a CRC checksum at the end of the message, calculated from
/	and including the message starting character '/' upto and including the message
/	end character '!'. According to the specs the CRC is a 16bit checksum using the
/	polynomial x^16 + x^15 + x^2 + 1, however input/output are reflected.
*/
bool P1MeterBase::CheckCRC()
{
	// sanity checks
	if (m_lbuffer[1] == 0)
	{
		if (m_p1version == 0)
		{
			Log(LOG_STATUS, "Meter is pre DSMR 4.0 and does not send a CRC checksum - using DSMR 2.2 compatibility");
			m_p1version = 2;
			m_applylimits = true;
		}
		// always return true with pre DSMRv4 format message
		return true;
	}

	if (m_lbuffer[5] != 0)
	{
		// trailing characters after CRC
		Log(LOG_NORM, "Dismiss incoming - CRC value in message has trailing characters");
		return false;
	}

	if (!m_CRfound)
	{
		Log(LOG_NORM, "You appear to have middle ware that changes the message content - skipping CRC validation");
		return true;
	}

	// retrieve CRC from the current line
	char crc_str[5];
	strncpy(crc_str, (const char*)&m_lbuffer + 1, 4);
	crc_str[4] = 0;
	uint16_t m_crc16 = (uint16_t)strtoul(crc_str, nullptr, 16);

	// calculate CRC
	uint16_t crc = 0;
	for (int ii = 0; ii < m_bufferpos; ii++)
	{
		crc = (crc >> 8) ^ p1_crc_16[(crc ^ m_buffer[ii]) & 0xFF];
	}
	if (crc == m_crc16)
		return true;

	Log(LOG_NORM, "Dismiss incoming - CRC failed");
	return false;
}


/*
/ GB3:	ParseP1Data() can be called with either a complete datagram (P1MeterTCP) or individual
/	lines (P1MeterSerial).
/
/	While it is technically possible to do a CRC check line by line, we like to keep
/	things organized and assemble the complete datagram before running that check. If the
/	datagram is DSMR 4.0+ of course.
/
/	Because older DSMR standard does not contain a CRC we still need the validation rules
/	in Matchline(). In fact, one of them is essential for keeping Oikomaticz from crashing
/	in specific cases of bad data. This means that a CRC check will only be done if the
/	datagram passes all other validation rules
*/
void P1MeterBase::ParseP1Data(const unsigned char *pData, const int Len, const bool disable_crc, int ratelimit)
{
	int ii = 0;
	m_ratelimit = ratelimit;
	// a new datagram should not start with an empty line, but just in case it does (crude check is sufficient here)
	while ((m_linecount == 0) && (pData[ii] < 0x10))
		ii++;

	// re enable reading pData when a new datagram starts, empty buffers
	if (!m_isencrypteddata && ((pData[ii] == 0x2f) || (pData[ii] == 0xdb)))
	{
		m_receivetime = mytime(nullptr);
		if (difftime(m_receivetime, m_lastUpdateTime) < m_ratelimit)
			return; // ignore this datagram

		if ((m_lbuffer[0] == 0x21) && !m_lexclmarkfound && (m_linecount > 0))
		{
			Log(LOG_NORM, "WARNING: got new datagram but buffer still contains unprocessed data from previous datagram.");
			m_lbuffer[m_lbufferpos] = 0;
			if (disable_crc || CheckCRC())
			{
				MatchLine();
			}
		}
		m_linecount = 1;
		m_lbufferpos = 0;
		m_bufferpos = 0;
		m_exclmarkfound = 0;
		if (pData[ii] == 0xdb)
		{
			m_isencrypteddata = true;
			m_p1gcmdata.pos = 0;
			m_p1gcmdata.datasize = 0;
			m_p1gcmdata.payloadend = 0;
			m_p1gcmdata.iv = "";
			m_p1gcmdata.payload = "";
			m_p1gcmdata.tag = "";
		}
	}

	if (m_linecount == 0)
		return;

	if (m_isencrypteddata)
		ParseP1EncryptedData(&pData[ii], (Len - ii), disable_crc);
	else
		ParseP1PlaintextData(&pData[ii], (Len - ii), disable_crc);
}


void P1MeterBase::ParseP1PlaintextData(const unsigned char *pData, const int Len, const bool disable_crc)
{
	int ii = 0;

	// read pData, ignore/stop if there is a datagram validation failure
	while ((ii < Len) && (m_linecount > 0) && (m_bufferpos < sizeof(m_buffer)))
	{
		const unsigned char c = pData[ii];
		ii++;

		if (!m_exclmarkfound)
		{
			// assemble complete datagram in message buffer for CRC validation
			m_buffer[m_bufferpos] = c;
			m_bufferpos++;
		}

		if (c == 0x21)
		{
			// stop writing to m_buffer after exclamation mark (do not include CRC)
			m_exclmarkfound = 1;
		}

		if (c == 0x0d)
		{
			m_CRfound = 1;
			continue;
		}

		if (c == 0x0a)
		{
			// close string, parse line and clear it.
			m_linecount++;
			if ((m_lbufferpos > 0) && (m_lbufferpos < sizeof(m_lbuffer)))
			{
				// don't try to match empty or oversized lines
				m_lbuffer[m_lbufferpos] = 0;
				if (m_lbuffer[0] == 0x21)
				{
					// exclamation mark signals datagram end, check CRC before allowing commit
					if (!disable_crc && !CheckCRC())
					{
						m_linecount = 0;
						return;
					}
				}
				if (!MatchLine())
				{
					// discard datagram
					m_linecount = 0;
				}
			}
			m_lbufferpos = 0;
		}
		else if (m_lbufferpos < sizeof(m_lbuffer))
		{
			m_lbuffer[m_lbufferpos] = c;
			m_lbufferpos++;
		}
	}

	if (m_bufferpos == sizeof(m_buffer))
	{
		// discard oversized datagram
		if ((Len > 600) || (pData[0] == 0x21))
		{
			// 400 is an arbitrary chosen number to differentiate between full datagrams and single line commits
			// the objective is that we only want to display this log line once for any datagram
			Log(LOG_NORM, "Dismiss incoming - datagram oversized");
		}
		m_linecount = 0;
		return;
	}
}


void P1MeterBase::ParseP1EncryptedData(const unsigned char *pData, const int Len, const bool disable_crc)
{
	for (int ii = 0; ((ii < Len) && (m_linecount > 0)); ii++)
	{
		m_p1gcmdata.pos++;
		if (m_p1gcmdata.pos == 1)
			continue;
		if (m_p1gcmdata.pos == 2)
		{
			// if system title size is not 8 then discard this telegram
			if (pData[ii] != 0x08)
			{
				m_linecount = 0;
				m_isencrypteddata = false;
			}
			continue;
		}
		if (m_p1gcmdata.pos < 11)
		{
			// 8 byte system title needs to got into the Initialization Vector property
			m_p1gcmdata.iv.insert(m_p1gcmdata.iv.end(), 1, pData[ii]);
			continue;
			}
		if (m_p1gcmdata.pos == 11)
		{
			// if separator value is not 0x82 then discard this telegram
			if (pData[ii] != 0x82)
			{
				m_linecount = 0;
				m_isencrypteddata = false;
			}
			continue;
		}
		if (m_p1gcmdata.pos == 12)
		{
			// bytes 12 and 13 give the number of remaining bytes in the message in big endian notation
			m_p1gcmdata.datasize = (pData[ii] << 8);
			ii++;
			m_p1gcmdata.pos++;
			m_p1gcmdata.datasize += pData[ii];
			m_p1gcmdata.payload.reserve((size_t)m_p1gcmdata.datasize);
			m_p1gcmdata.datasize += ii; // header size = 13
			m_p1gcmdata.payloadend = m_p1gcmdata.datasize - 12;
			continue;
		}
		if (m_p1gcmdata.pos == 14)
		{
			// if separator value is not 0x30 then discard this telegram
			if (pData[ii] != 0x30)
			{
				m_linecount = 0;
				m_isencrypteddata = false;
			}
			continue;
		}
		if (m_p1gcmdata.pos < 18)
		{
			// 4 byte sequence number must be appended to the Initialization Vector property
			m_p1gcmdata.iv.insert(m_p1gcmdata.iv.end(), 1, pData[ii]);
			continue;
		}
		if (m_p1gcmdata.pos < m_p1gcmdata.payloadend)
		{
			// payload
			m_p1gcmdata.payload.insert(m_p1gcmdata.payload.end(), 1, pData[ii]);
			continue;
		}
		if (m_p1gcmdata.pos < m_p1gcmdata.datasize)
		{
			// GCM tag
			m_p1gcmdata.payload.insert(m_p1gcmdata.payload.end(), 1, pData[ii]);
			m_p1gcmdata.tag.insert(m_p1gcmdata.tag.end(), 1, pData[ii]);
			continue;
		}
	}

	if (m_p1gcmdata.tag.size() == 12)
	{
		// telegram is complete - decode and send to plain text parser

		uint8_t* decryptedData = new uint8_t[m_p1gcmdata.payload.size() + 16];
		int decryptedsize = 0;

		try
		{

			EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
			if (ctx == nullptr)
					return;
			EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr);
			EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, m_p1gcmdata.iv.size(), nullptr);

			EVP_DecryptInit_ex(ctx, nullptr, nullptr, (const unsigned char*)m_decryptkey.c_str(), (const unsigned char*)m_p1gcmdata.iv.c_str());

			unsigned char AddAuthData[] = {0x30, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
			EVP_DecryptUpdate(ctx, nullptr, &decryptedsize, (const unsigned char*) AddAuthData, 17);

			EVP_DecryptUpdate(ctx, (unsigned char*)decryptedData, &decryptedsize, (unsigned char*)m_p1gcmdata.payload.c_str(), m_p1gcmdata.payload.size());
			EVP_CIPHER_CTX_free(ctx);
		}
		catch (const std::exception& e)
		{
			Log(LOG_ERROR, "Error decrypting payload (%s)", e.what());
		}


		if (decryptedsize <= 0)
			return;
		decryptedData[decryptedsize] = 0;

		m_isencrypteddata = false;
		ParseP1PlaintextData(decryptedData, decryptedsize, disable_crc);

		delete[] decryptedData;
	}
}


void P1MeterBase::UpsertSwitch(const int NodeID, const device::tswitch::type::value switchtype, const int switchstate, const char* defaultname)
{
	char szID[10];
	sprintf(szID, "%08X", (unsigned int)NodeID);
	unsigned char unit = 1;

	//Check if we already exist
	std::vector<std::vector<std::string> > result;
	result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (DeviceID='%q') AND (Unit=%d) AND (Type=%d) AND (SubType=%d) AND (SwitchType=%d)",
		m_HwdID, szID, int(unit), pTypeGeneralSwitch, sSwitchGeneralContact, int(switchtype));

	GeneralSwitch pSwitch;
	pSwitch.type = pTypeGeneralSwitch;
	pSwitch.subtype = sSwitchGeneralContact;
	pSwitch.id = NodeID;
	pSwitch.unitcode = unit;
	pSwitch.cmnd = switchstate;
	pSwitch.seqnbr = 0;
	m_mainworker.PushAndWaitRxMessage(this, (const unsigned char *)&pSwitch, defaultname, 255, m_Name.c_str());

	if (result.empty())
	{
		// wait a maximum of 1 second for mainworker to finish adding the device
		int i=10;
		while (i && result.empty())
		{
			result = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID=%d) AND (DeviceID='%q') AND (Unit=%d) AND (Type=%d) AND (SubType=%d)",
				m_HwdID, szID, int(unit), pTypeGeneralSwitch, sSwitchGeneralContact);

			if (result.empty())
			{
				sleep_milliseconds(100);
				i--;
			}
		}

		//Set SwitchType and CustomImage
		int iconID = CUSTOM_IMAGE_ID;
		m_sql.safe_query("UPDATE DeviceStatus SET SwitchType=%d, CustomImage=%d WHERE (HardwareID=%d) AND (DeviceID='%q') AND (Unit=%d) AND (Type=%d) AND (SubType=%d)", int(switchtype), iconID, m_HwdID, szID, int(unit), pTypeGeneralSwitch, sSwitchGeneralContact);
	}
}


bool P1MeterBase::SetOptions(const bool disable_crc, const unsigned int ratelimit, const unsigned int gasmbuschannel)
{
	if (gasmbuschannel > 5)
		return false;

	if (gasmbuschannel > 0)
	{
		unsigned char cmbuschannel = static_cast<unsigned char>(gasmbuschannel) | 0x30;
		if (cmbuschannel != m_privgas.channel)
		{
			Log(LOG_STATUS, "Gas meter M-Bus channel %c manually set by user", cmbuschannel);
			m_privgas.channel = cmbuschannel;
		}
	}
	else
		m_privgas.channel = 0;

	m_bDisableCRC = disable_crc;
	m_applylimits = disable_crc;
	m_ratelimit = ratelimit;
	return true;
}


bool P1MeterBase::ImportKey(std::string szhexencoded)
{
	m_decryptkey = "";
	if (szhexencoded.empty()) // nothing to do
		return true;
	uint8_t byte;
	bool validkey = (!(szhexencoded.size() & 0x1));
	for (int i = 0; (i < static_cast<int>(szhexencoded.size())) && validkey; i++)
	{
		uint8_t c = szhexencoded[i] | 0x20;
		// valid chars 0x30-0x39, 0x61-0x67
		if (c & 0x10)
			c -= 0x30; // 0-9
		else
			c -= 87; // 10-15
		if (c & 0xF0)
		{
			m_decryptkey = "";
			validkey = false;
		}
		if (!(i & 0x1))
			byte = c << 4;
		else
		{
			byte += c;
			m_decryptkey.insert(m_decryptkey.end(), 1, (char)byte);
		}
	}
	if (!validkey)
	{
		Log(LOG_ERROR, "Invalid decryption key in hardware settings");
		return false;
	}
	return true;
}

time_t P1MeterBase::ParseP1datetime(const std::string &szP1datetime)
{
	struct tm gastm;
	gastm.tm_year = atoi(szP1datetime.substr(0, 2).c_str()) + 100;
	gastm.tm_mon = atoi(szP1datetime.substr(2, 2).c_str()) - 1;
	gastm.tm_mday = atoi(szP1datetime.substr(4, 2).c_str());
	gastm.tm_hour = atoi(szP1datetime.substr(6, 2).c_str());
	gastm.tm_min = atoi(szP1datetime.substr(8, 2).c_str());
	gastm.tm_sec = atoi(szP1datetime.substr(10, 2).c_str());
	if (szP1datetime.length() == 12)
		gastm.tm_isdst = -1;
	else if (szP1datetime[12] == 'W')
		gastm.tm_isdst = 0;
	else
		gastm.tm_isdst = 1;

	return mktime(&gastm);
}

//Webserver helpers
namespace http {
	namespace server {
		void CWebServer::Cmd_P1SetOptions(WebEmSession & session, const request& req, Json::Value &root)
		{
			if (session.rights != 2)
			{
				session.reply_status = reply::forbidden;
				return; //Only admin user allowed
			}
			std::string hwid = request::findValue(&req, "idx");
			std::string mode2 = request::findValue(&req, "mode2");
			std::string mode3 = request::findValue(&req, "mode3");
			std::string mode4 = request::findValue(&req, "mode4");
			if (
				(hwid == "") ||
				(mode2 == "") ||
				(mode3 == "") ||
				(mode4 == "")
				)
				return;
			int iHardwareID = atoi(hwid.c_str());
			CDomoticzHardwareBase *pBaseHardware = m_mainworker.GetHardware(iHardwareID);
			if (pBaseHardware == nullptr)
				return;
			if ((pBaseHardware->HwdType != hardware::type::P1SmartMeter) && (pBaseHardware->HwdType != hardware::type::P1SmartMeterLAN))
				return;
			P1MeterBase *pHardware = reinterpret_cast<P1MeterBase*>(pBaseHardware);

			int iMode2 = atoi(mode2.c_str());
			int iMode3 = atoi(mode3.c_str());
			int iMode4 = atoi(mode4.c_str());

			if (pHardware->SetOptions((iMode2 != 0), iMode3, iMode4))
			{
				m_sql.safe_query("UPDATE Hardware SET Mode2=%d,Mode3=%d,Mode4=%d WHERE (ID == '%q')", iMode2, iMode3, iMode4, hwid.c_str());
				root["status"] = "OK";
				root["title"] = "P1SetOptions";
			}
		}
	}
}
