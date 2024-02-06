/*
 *  Client interface for APSystems local ECU device access
 *
 *  Copyright 2024 - gordonb3 https://github.com/gordonb3/apsystems-ecupp
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 *  @license GPL-3.0+ <https://github.com/gordonb3/apsystems-ecupp/blob/master/LICENSE>
 */

// APSystems ECU Class
#ifndef _ecuAPI
#define _ecuAPI

#include <vector>
#include <string>
#include <time.h>
#include <cstdint>


namespace APSystems {
  namespace ecu {

    typedef struct _sInverterChannel
    {
      int power;
      int volt;
    } InverterChannel;

    typedef struct _sInverterInfo
    {
      std::string id;
      uint8_t online_status;
      double frequency;
      int temperature;
      int signal_strength;
      std::vector<APSystems::ecu::InverterChannel> channels;
    } InverterInfo;

    typedef struct _sSystemInfo
    {
      std::string id;
      std::string version;
      int current_power;
      time_t timestamp;
      double today_energy;
      double lifetime_energy;
      int numinverters;
      int invertersonline;
      std::vector<APSystems::ecu::InverterInfo> inverters;
    } SystemInfo;

  }; // namespace ecu
}; // namespace APSystems



class ecuAPI
{

public:
	ecuAPI();
	~ecuAPI();

	APSystems::ecu::SystemInfo m_apsecu;

	void SetTargetAddress(const std::string ip_address);

	int QueryECU();
	int QueryInverters();
	int GetInverterSignalLevels();
	int GetDayReport(const int year, const uint8_t month, const uint8_t day, std::string &jsondata);
	int GetPeriodReport(const uint8_t period, std::string &jsondata);


private:
	int m_sockfd;
	std::string m_ecu_address;

	const uint8_t MESSAGE_SEND_TRAILER[4] = {'E','N','D',0x0a};

	bool VerifyMessageSize(const int bytes_received, const unsigned char* buffer);
	int ReadBigMachineInt(const unsigned char* buffer, const int pos);
	int ReadBigMachineSmallInt(const unsigned char* buffer, const int pos);
	std::string ReadBCDnumber(const unsigned char* buffer, const int pos, const int numbytes);
	time_t ReadTimestamp(const unsigned char* buffer, const int pos);

	bool ResolveHost(struct sockaddr_in& serv_addr);
	bool ConnectToDevice();
	int send(unsigned char* buffer, const unsigned int size);
	int receive(unsigned char* buffer, const unsigned int maxsize);
	void disconnect();
};

#endif

