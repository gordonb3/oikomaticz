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

#define SOCKET_TIMEOUT_SECS 1
#define ECU_LISTEN_PORT 8899
#define READ_BUFFER_SIZE 2000

#define ECU_QUERY_HEADER 	"APS1100160001"
#define INVERTER_QUERY_HEADER	"APS1100280002"
#define INVERTER_SIGNAL_HEADER	"APS1100280003"
#define GET_ENERGY_DAY		"APS1100360003"
#define GET_ENERGY_WMY		"APS1100300004"

#include "ecuAPI.hpp"
#include <netdb.h>
#include <zlib.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>

#ifdef WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef DEBUG
#include <iostream>

void exit_error(const char *msg)
{
	perror(msg);
	exit(0);
}
#endif

ecuAPI::ecuAPI()
{
}


ecuAPI::~ecuAPI()
{
	disconnect();
}


void ecuAPI::SetTargetAddress(const std::string ip_address)
{
	m_ecu_address = ip_address;
}


int ecuAPI::GetDayReport(const int year, const uint8_t month, const uint8_t day, std::string &jsondata)
{
	unsigned char buffer[READ_BUFFER_SIZE] = GET_ENERGY_DAY;
	int buffer_pos = 13;
	bcopy(m_apsecu.id.c_str(), (char*)&buffer[buffer_pos], m_apsecu.id.length());
	buffer_pos += m_apsecu.id.length();
	bcopy(MESSAGE_SEND_TRAILER, (char*)&buffer[buffer_pos], 3);
	buffer_pos += 3;

	char datestring[20];
	sprintf(datestring, "%04d%02d%02d\n", year, month, day);
	bcopy(datestring, (char*)&buffer[buffer_pos], 9);
	buffer_pos += 9;

	if (!ConnectToDevice())
		return -1;
	send(buffer, buffer_pos);
	int numbytes = receive(buffer, READ_BUFFER_SIZE);
	disconnect();

#ifdef DEBUG
	std::cout << "dbg: received message: ";
	for(int i=0; i<(int)(numbytes); ++i)
		printf("%.2x", (uint8_t)buffer[i]);
	std::cout << "\n";
#endif

	if ( (numbytes < 15) || (!VerifyMessageSize(numbytes, buffer)) )
		return 1;

	int pos = 15;
	int maxpos = numbytes - 4;
	if (maxpos == pos)
	{
		jsondata = "{\n    datapoints : []\n}\n";
		return 0;
	}
	jsondata = "{\n    datapoints :\n    [";
	while (pos < maxpos)
	{
		unsigned char* currenttime = &buffer[pos];
		std::string hour = ReadBCDnumber(currenttime, 0, 1);
		std::string minute = ReadBCDnumber(currenttime, 1, 1);
		int power = ReadBigMachineSmallInt(currenttime, 2);
		if (pos > 15)
			jsondata.append(",");
		char dayresult[100];
		sprintf(dayresult, "\n        {\n            \"time\" : \"%s:%s\"\n            \"power\" : %d\n        }", hour.c_str(), minute.c_str(), power);
		jsondata.append(dayresult);
		pos += 4;
	}
	jsondata.append("\n    ]\n}\n");

	return 0;
}


int ecuAPI::GetPeriodReport(const uint8_t period, std::string &jsondata)
{
	if (period > 2)
		return -1;

	unsigned char buffer[READ_BUFFER_SIZE] = GET_ENERGY_WMY;
	int buffer_pos = 13;
	bcopy(m_apsecu.id.c_str(), (char*)&buffer[buffer_pos], m_apsecu.id.length());
	buffer_pos += m_apsecu.id.length();
	bcopy(MESSAGE_SEND_TRAILER, (char*)&buffer[buffer_pos], 3);
	buffer_pos += 3;
	uint8_t datestring[3] = { 0x30, (uint8_t)(period | 0x30), 0x0a };
	bcopy(datestring, (char*)&buffer[buffer_pos], 3);
	buffer_pos += 3;


	if (!ConnectToDevice())
		return -1;
	send(buffer, buffer_pos);
	int numbytes = receive(buffer, READ_BUFFER_SIZE);
	disconnect();

	if ( (numbytes < 15) || (!VerifyMessageSize(numbytes, buffer)) )
		return 1;

	int pos = 17;
	int maxpos = numbytes - 4;
	if (maxpos == pos)
	{
		jsondata = "{\n    datapoints : []\n}\n";
		return 0;
	}
	jsondata = "{\n    datapoints :\n    [";
	while (pos < maxpos)
	{
		unsigned char* currenttime = &buffer[pos];
		std::string year = ReadBCDnumber(currenttime, 0, 2);
		std::string month = ReadBCDnumber(currenttime, 2, 1);
		std::string day = ReadBCDnumber(currenttime, 3, 1);
		double energy = static_cast<double>(ReadBigMachineInt(currenttime, 4)) / 100;
		if (pos > 17)
			jsondata.append(",");
		char dayresult[100];
		sprintf(dayresult, "\n        {\n            \"date\" : \"%s-%s-%s\"\n            \"power\" : %0.2f\n        }", year.c_str(), month.c_str(), day.c_str(), energy);
		jsondata.append(dayresult);
		pos += 8;
	}
	jsondata.append("\n    ]\n}\n");
	
	return 0;
}


int ecuAPI::QueryECU()
{
	unsigned char buffer[READ_BUFFER_SIZE] = ECU_QUERY_HEADER;
	int buffer_pos = 13;
	bcopy(MESSAGE_SEND_TRAILER, (char*)&buffer[buffer_pos], sizeof(MESSAGE_SEND_TRAILER));
	buffer_pos += (int)sizeof(MESSAGE_SEND_TRAILER);

	if (!ConnectToDevice())
		return -1;
	send(buffer, buffer_pos);
	int numbytes = receive(buffer, READ_BUFFER_SIZE);
	disconnect();

#ifdef DEBUG
	std::cout << "dbg: received message: ";
	for(int i=0; i<(int)(numbytes); ++i)
		printf("%.2x", (uint8_t)buffer[i]);
	std::cout << "\n";
#endif
	if ( (numbytes < 45) || (!VerifyMessageSize(numbytes, buffer)) )
		return 1;

	if (m_apsecu.id.empty())
		m_apsecu.id.append((const char*)&buffer[13],12);

	m_apsecu.timestamp = -1;
	m_apsecu.lifetime_energy = static_cast<double>(ReadBigMachineInt(buffer,27)) / 10;
	m_apsecu.current_power = ReadBigMachineInt(buffer,31);
	m_apsecu.today_energy = static_cast<double>(ReadBigMachineInt(buffer,35)) / 100;

	if (buffer[25] == '0')
	{
		if (buffer[26] == '1')	// 
		{
			m_apsecu.numinverters = ReadBigMachineSmallInt(buffer,46);
			m_apsecu.invertersonline = ReadBigMachineSmallInt(buffer,48);
			std::string vlenstring;
			vlenstring.append((const char*)&buffer[52],3);
			int vlen = atoi(vlenstring.c_str());
			std::string versionstring;
			versionstring.append((const char*)&buffer[55],vlen);
			m_apsecu.version = versionstring;

		}
		else if (buffer[26] == '2')  // 
		{
			m_apsecu.numinverters = ReadBigMachineSmallInt(buffer,39);
			m_apsecu.invertersonline = ReadBigMachineSmallInt(buffer,41);
			std::string vlenstring;
			vlenstring.append((const char*)&buffer[49],3);
			int vlen = atoi(vlenstring.c_str());
			std::string versionstring;
			versionstring.append((const char*)&buffer[52],vlen);
			m_apsecu.version = versionstring;
		}
		else return -1;
	}

	// reset the list of inverters
	std::vector<APSystems::ecu::InverterInfo>().swap(m_apsecu.inverters);
	if (m_apsecu.numinverters > 0)
		m_apsecu.inverters.resize(m_apsecu.numinverters);

	return 0;
}


int ecuAPI::QueryInverters()
{
	if (m_apsecu.id.empty())
	{
		int statuscode = QueryECU();
		if (statuscode != 0)
			return statuscode;
	}
	unsigned char buffer[READ_BUFFER_SIZE] = INVERTER_QUERY_HEADER;
	int buffer_pos = 13;
	bcopy(m_apsecu.id.c_str(), (char*)&buffer[buffer_pos], m_apsecu.id.length());
	buffer_pos += m_apsecu.id.length();
	bcopy(MESSAGE_SEND_TRAILER, (char*)&buffer[buffer_pos], sizeof(MESSAGE_SEND_TRAILER));
	buffer_pos += (int)sizeof(MESSAGE_SEND_TRAILER);

	if (!ConnectToDevice())
		return -1;
	send(buffer, buffer_pos);
	int numbytes = receive(buffer, READ_BUFFER_SIZE);
	disconnect();

#ifdef DEBUG
	std::cout << "dbg: received message: ";
	for(int i=0; i<(int)(numbytes); ++i)
		printf("%.2x", (uint8_t)buffer[i]);
	std::cout << "\n";
#endif
	if ( (numbytes < 30) || (!VerifyMessageSize(numbytes, buffer)) )
		return 1;

	m_apsecu.timestamp = ReadTimestamp(buffer, 19);

	int inverter_pos = 26;
	int maxpos = numbytes - 9;
	for (int i = 0; ( (i < m_apsecu.numinverters) && (inverter_pos < maxpos) ); i++)
	{
		unsigned char* currentinverter = &buffer[inverter_pos];
		m_apsecu.inverters[i].id = ReadBCDnumber(currentinverter, 0, 6);
		m_apsecu.inverters[i].online_status = currentinverter[6];

		if (m_apsecu.inverters[i].online_status == 0)
		{
			m_apsecu.inverters[i].temperature = 0;
			m_apsecu.inverters[i].frequency = 0;
		}

		// reset the list of channels
		std::vector<APSystems::ecu::InverterChannel>().swap(m_apsecu.inverters[i].channels);

		if (currentinverter[7] == '0')
		{
			if (currentinverter[8] == '1')		// YC600/DS3 series
			{
				inverter_pos += 21;
				if (inverter_pos > numbytes)
					return -1;
				if (m_apsecu.inverters[i].online_status != 0)
				{
					m_apsecu.inverters[i].frequency = static_cast<double>(ReadBigMachineSmallInt(currentinverter, 9)) / 10;
					m_apsecu.inverters[i].temperature = ReadBigMachineSmallInt(currentinverter, 11) - 100;

					m_apsecu.inverters[i].channels.resize(2);
					int channel_pos = 13;
					for (int j = 0; j < 2; j++)
					{
						unsigned char* currentchannel = &currentinverter[channel_pos];
						m_apsecu.inverters[i].channels[j].power = ReadBigMachineSmallInt(currentchannel, 0);
						if (j == 0)
							m_apsecu.inverters[i].channels[j].volt = ReadBigMachineSmallInt(currentchannel, 2);
						else
							m_apsecu.inverters[i].channels[j].volt = m_apsecu.inverters[i].channels[0].volt;
						channel_pos += 4;
					}
				}
			}
			else if (currentinverter[8] == '2')		// YC1000/QT2 series
			{
				inverter_pos += 27;
				if (inverter_pos > numbytes)
					return -1;
				if (m_apsecu.inverters[i].online_status != 0)
				{
					m_apsecu.inverters[i].frequency = static_cast<double>(ReadBigMachineSmallInt(currentinverter, 9)) / 10;
					m_apsecu.inverters[i].temperature = ReadBigMachineSmallInt(currentinverter, 11) - 100;

					m_apsecu.inverters[i].channels.resize(4);
					int channel_pos = 13;
					for (int j = 0; j < 4; j++)
					{
						unsigned char* currentchannel = &currentinverter[channel_pos];
						m_apsecu.inverters[i].channels[j].power = ReadBigMachineSmallInt(currentchannel, 0);
						if (j == 0)
							m_apsecu.inverters[i].channels[j].volt = ReadBigMachineSmallInt(currentchannel, 2);
						else
							m_apsecu.inverters[i].channels[j].volt = m_apsecu.inverters[i].channels[0].volt;
						channel_pos += 4;
					}
				}
			}
			else if (currentinverter[8] == '3')		// QS1
			{
				inverter_pos += 23;
				if (inverter_pos > numbytes)
					return -1;
				if (m_apsecu.inverters[i].online_status != 0)
				{
					m_apsecu.inverters[i].frequency = static_cast<double>(ReadBigMachineSmallInt(currentinverter, 9)) / 10;
					m_apsecu.inverters[i].temperature = ReadBigMachineSmallInt(currentinverter, 11) - 100;

					m_apsecu.inverters[i].channels.resize(4);
					int channel_pos = 13;
					for (int j = 0; j < 4; j++)
					{
						unsigned char* currentchannel = &currentinverter[channel_pos];
						m_apsecu.inverters[i].channels[j].power = ReadBigMachineSmallInt(currentchannel, 0);
						if (j == 0)
						{
							m_apsecu.inverters[i].channels[j].volt = ReadBigMachineSmallInt(currentchannel, 2);
							channel_pos += 4;
						}
						else
						{
							m_apsecu.inverters[i].channels[j].volt = m_apsecu.inverters[i].channels[0].volt;
							channel_pos += 2;
						}
					}
				}
			}
			else
				inverter_pos += 9;
		}
	}
	return 0;
}


int ecuAPI::GetInverterSignalLevels()
{
	if (m_apsecu.id.empty() || (m_apsecu.numinverters == 0))
	{
		int statuscode = QueryInverters();
		if (statuscode != 0)
			return statuscode;
	}
	unsigned char buffer[READ_BUFFER_SIZE] = INVERTER_SIGNAL_HEADER;
	int buffer_pos = 13;
	bcopy(m_apsecu.id.c_str(), (char*)&buffer[buffer_pos], m_apsecu.id.length());
	buffer_pos += m_apsecu.id.length();
	bcopy(MESSAGE_SEND_TRAILER, (char*)&buffer[buffer_pos], sizeof(MESSAGE_SEND_TRAILER));
	buffer_pos += (int)sizeof(MESSAGE_SEND_TRAILER);

	if (!ConnectToDevice())
		return -1;
	send(buffer, buffer_pos);
	int numbytes = receive(buffer, READ_BUFFER_SIZE);
	disconnect();

#ifdef DEBUG
	std::cout << "dbg: received message: ";
	for(int i=0; i<(int)(numbytes); ++i)
		printf("%.2x", (uint8_t)buffer[i]);
	std::cout << "\n";
#endif
	if ( (numbytes < 15) || (!VerifyMessageSize(numbytes, buffer)) )
		return 1;

	int inverter_pos = 15;
	int maxpos = numbytes - 4;
	while (inverter_pos < maxpos)
	{
		unsigned char* currentinverter = &buffer[inverter_pos];
		std::string inverter_id = ReadBCDnumber(currentinverter, 0, 6);
		for (int j = 0; j < m_apsecu.numinverters; j++)
		{
			if (m_apsecu.inverters[j].id == inverter_id)
			{
				m_apsecu.inverters[j].signal_strength = (int)(currentinverter[6] * 100 / 255);
				break;
			}
		}
		inverter_pos += 7;
	}
	return 0;
}


/* private */ int ecuAPI::ReadBigMachineInt(const unsigned char* buffer, const int pos)
{
	const unsigned char* cursor = &buffer[pos];
	return ((uint8_t)cursor[0] << 24) + ((uint8_t)cursor[1] << 16) + ((uint8_t)cursor[2] << 8) + ((uint8_t)cursor[3]);
}


/* private */ int ecuAPI::ReadBigMachineSmallInt(const unsigned char* buffer, const int pos)
{
	const unsigned char* cursor = &buffer[pos];
	return ((uint8_t)cursor[0] << 8) + ((uint8_t)cursor[1]);
}


/* private */ std::string ecuAPI::ReadBCDnumber(const unsigned char* buffer, const int pos, const int numbytes)	// note: 1 byte is 2 BCD digits
{
	const unsigned char* BCDnumber = &buffer[pos];
	const char HEX_CHARS[] = "0123456789abcdef";
	std::string result;
	for (int i = 0; i < numbytes; i++)
	{
		result.insert(result.end(),HEX_CHARS[(BCDnumber[i] >> 4)]);
		result.insert(result.end(),HEX_CHARS[(BCDnumber[i] & 0x0F)]);
	}
	return result;
}


/* private */ time_t ecuAPI::ReadTimestamp(const unsigned char* buffer, const int pos)
{
	const unsigned char* BCDnumber = &buffer[pos];
	struct tm timestamp;
	timestamp.tm_year = (BCDnumber[0] >> 4) * 1000 + (BCDnumber[0] & 0x0F) * 100 + (BCDnumber[1] >> 4) * 10 + (BCDnumber[1] & 0x0F) - 1900;
	timestamp.tm_mon = (BCDnumber[2] >> 4) * 10 + (BCDnumber[2] & 0x0F) - 1;
	timestamp.tm_mday = (BCDnumber[3] >> 4) * 10 + (BCDnumber[3] & 0x0F);
	timestamp.tm_hour = (BCDnumber[4] >> 4) * 10 + (BCDnumber[4] & 0x0F);
	timestamp.tm_min = (BCDnumber[5] >> 4) * 10 + (BCDnumber[5] & 0x0F);
	timestamp.tm_sec = (BCDnumber[6] >> 4) * 10 + (BCDnumber[6] & 0x0F);
	timestamp.tm_isdst = -1;
	return mktime(&timestamp);
}


/* private */ bool ecuAPI::VerifyMessageSize(const int bytes_received, const unsigned char* buffer)
{
	int check_size = bytes_received - 1;
	std::string sizestring;
	sizestring.append((const char*)&buffer[5],4);
	int reported_size = stoi(sizestring);
	return (check_size == reported_size);
}


/* private */ bool ecuAPI::ResolveHost(struct sockaddr_in& serv_addr)
{
	if ((m_ecu_address[0] ^ 0x30) < 10)
	{
		serv_addr.sin_family = AF_INET;
		if (inet_pton(AF_INET, m_ecu_address.c_str(), &serv_addr.sin_addr) == 1)
			return true;
	}
	if (m_ecu_address.find(':') != std::string::npos)
	{
		serv_addr.sin_family = AF_INET6;
		if (inet_pton(AF_INET6, m_ecu_address.c_str(), &serv_addr.sin_addr) == 1)
			return true;
	}
	struct addrinfo *addr;
	if (getaddrinfo(m_ecu_address.c_str(), "0", nullptr, &addr) == 0)
	{
		struct sockaddr_in *saddr = (((struct sockaddr_in *)addr->ai_addr));
		memcpy(&serv_addr, saddr, sizeof(sockaddr_in));
		return true;
	}

	return false;
}


/* private */ bool ecuAPI::ConnectToDevice()
{
	struct sockaddr_in serv_addr;
	bzero((char*)&serv_addr, sizeof(serv_addr));
	if (!ResolveHost(serv_addr))
		return false;

#ifdef WIN32
	m_sockfd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0 , 0 , 0);
#else
	m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
#endif
	if (m_sockfd < 0)
		return false;

	serv_addr.sin_port = htons(ECU_LISTEN_PORT);

#ifdef WIN32
	int timeout = SOCKET_TIMEOUT_SECS * 1000;
#else
	struct timeval timeout;
	timeout.tv_sec = SOCKET_TIMEOUT_SECS;
	timeout.tv_usec = 0;
#endif
	setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

	if (connect(m_sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0)
		return true;

	return false;
}


/* private */ int ecuAPI::send(unsigned char* buffer, const unsigned int size)
{
#ifdef WIN32
	return send(m_sockfd, buffer, size, 0);
#else
	return write(m_sockfd, buffer, size);
#endif
}


/* private */ int ecuAPI::receive(unsigned char* buffer, const unsigned int maxsize)
{
#ifdef WIN32
	return recv(m_sockfd, buffer, maxsize, 0 );
#else
	return read(m_sockfd, buffer, maxsize);
#endif
}


/* private */ void ecuAPI::disconnect()
{
#ifdef WIN32
	closesocket(m_sockfd);
#else
	close(m_sockfd);
#endif
	m_sockfd = 0;
}

