#pragma once

#ifndef WIN32
#ifdef WITH_LIBUSB
#include "DomoticzHardware.h"

class CVoltCraftCO20 : public CDomoticzHardwareBase
{
public:
	explicit CVoltCraftCO20(const int ID);
	~CVoltCraftCO20(void);

	bool WriteToHardware(const char *pdata, const unsigned char length) override;
private:
	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();
	void GetSensorDetails();
private:
	std::shared_ptr<std::thread> m_thread;
};

#endif //WITH_LIBUSB
#endif //WIN32
