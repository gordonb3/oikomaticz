/*
 *  Client interface for local Tuya device access
 *
 *  Copyright 2022-2024 - gordonb3 https://github.com/gordonb3/tuyapp
 *
 *  Licensed under GNU General Public License 3.0 or later.
 *  Some rights reserved. See COPYING, AUTHORS.
 *
 *  @license GPL-3.0+ <https://github.com/gordonb3/tuyapp/blob/master/LICENSE>
 */

// Tuya TCP Class

#ifndef _tuyaTCP
#define _tuyaTCP

// Tuya Local Access TCP Port
#define TUYA_COMMAND_PORT 6668

#include <string>
#include <cstdint>


namespace Tuya {
  namespace TCP {
    namespace Socket {
      enum value {
        NO_SUCH_HOST,
        NO_SOCK_AVAIL,
        FAILED,
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
	READY,
        RECEIVING
      }; // enum value
    }; // namespace Socket
  }; // namespace TCP
}; // namespace Tuya


class tuyaTCP
{

public:
	tuyaTCP();
	~tuyaTCP();

	void setAsyncMode(bool async = true);
	Tuya::TCP::Socket::value getSocketState();

	virtual bool ConnectToDevice(const std::string &hostname, const uint8_t retries = 1);
	int send(unsigned char* buffer, const int size);
	int receive(unsigned char* buffer, const int maxsize, const int minsize = 28);
	int getlasterror();
	void disconnect();
	bool isSocketWritable();
	bool isSocketReadable();
	bool setSessionReady();

protected:
	Tuya::TCP::Socket::value m_socketState;

private:
	int getSocketEvents(short events, int timeout);

	int m_sockfd;
	int m_lasterror;
	bool m_asyncMode;
};

#endif

