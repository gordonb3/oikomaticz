#pragma once

#include "hardware/DomoticzHardware.h"
#include "hardware/hardwaretypes.h"

class P1MeterBase : public CDomoticzHardwareBase
{
	friend class P1MeterSerial;
	friend class P1MeterTCP;
	friend class CRFXBase;

      public:
	P1MeterBase();
	~P1MeterBase() override;

	bool SetOptions(bool disable_crc, unsigned int ratelimit, unsigned int gasmbuschannel);

      private:
	void Init();
	bool MatchLine();
	void ParseP1Data(const uint8_t *pData, int Len, bool disable_crc, int ratelimit);
	void ParseP1PlaintextData(const uint8_t *pData, int Len, bool disable_crc);

	bool CheckCRC();
	void UpsertSwitch(int NodeID, const device::tswitch::type::value switchtype, int switchstate, const char* defaultname);
	time_t ParseP1datetime(const std::string &szP1datetime);

	// Luxembourgian Smarty encrypted telegram support
	bool ImportKey(std::string szhexencoded);
	void ParseP1EncryptedData(const uint8_t *pData, int Len, bool disable_crc);

      private:
	P1Power	m_power;
	P1BusDevice m_gas;
	P1BusDevice m_water;
	P1BusDevice m_thermal;

      protected:
	bool m_bDisableCRC;
	int m_ratelimit;
	bool m_applylimits;

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
	bool m_mbusonly;

	typedef struct _mbusdata
	{
		unsigned char channel;
		unsigned long lastusage;
		time_t lastsubmittime;
		time_t nextmetertime;
		double clockskew;
		char timestring[16];
	} mbusdata;

	mbusdata m_privgas;
	mbusdata m_privwater;
	mbusdata m_privthermal;


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
