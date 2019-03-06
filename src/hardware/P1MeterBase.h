#pragma once

#include "hardware/DomoticzHardware.h"
#include "hardware/hardwaretypes.h"

class P1MeterBase : public CDomoticzHardwareBase
{
	friend class P1MeterSerial;
	friend class P1MeterTCP;
	friend class CRFXBase;

public:
	P1MeterBase(void);
	~P1MeterBase(void);

	bool SetOptions(const bool disable_crc, const unsigned int ratelimit, const unsigned int gasmbuschannel);

private:
	void Init();
	bool MatchLine();
	void ParseP1Data(const unsigned char *pData, const int Len, const bool disable_crc, int ratelimit);

	bool CheckCRC();
	void UpsertSwitch(const int NodeID, const device::_switch::type::value switchtype, const int switchstate, const char* defaultname);

public:
	P1Power	m_power;
	P1Gas	m_gas;

protected:
	bool m_bDisableCRC;
	unsigned int m_ratelimit;

private:
	typedef struct _tP1PhaseData
	{
		float voltage[4];
		float instpwrdel[4];
		float instpwruse[4];
	} P1PhaseData;


	P1PhaseData m_phasedata;

	unsigned char m_buffer[1400];
	int m_bufferpos;
	unsigned char m_exclmarkfound;

	unsigned char m_lbuffer[128];
	int m_lbufferpos;
	unsigned char m_lexclmarkfound;

	time_t m_lastUpdateTime;
	time_t m_receivetime;
	unsigned char m_p1version;
	unsigned char m_phasecount;
	unsigned char m_linecount;
	unsigned char m_CRfound;
	float m_currentTariff;
	float m_lastTariff;

	unsigned char m_gasmbuschannel;
	unsigned long m_lastgasusage;
	std::string m_gastimestamp;
	time_t m_lastSharedSendGas;
	time_t m_gasoktime;
	double m_gasclockskew;
};
