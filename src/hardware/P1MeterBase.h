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
	void ParseP1PlaintextData(const unsigned char *pData, const int Len, const bool disable_crc);

	bool CheckCRC();
	void UpsertSwitch(const int NodeID, const device::tswitch::type::value switchtype, const int switchstate, const char* defaultname);

	// Luxembourgian Smarty encrypted telegram support
	bool ImportKey(std::string szhexencoded);
	void ParseP1EncryptedData(const unsigned char *pData, const int Len, const bool disable_crc);

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
		float ampere[4];
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


	// Luxembourgian Smarty encrypted telegram support
	bool m_isencrypteddata;
	std::string m_decryptkey;

	typedef struct _tAES_GCM_data
	{
		int pos;
		int datasize;
		int payloadend;
		std::string iv;
		std::string payload;
		std::string tag;
	} AES_GCM_data;

	AES_GCM_data m_p1gcmdata;

};
