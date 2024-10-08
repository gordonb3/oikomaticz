#include "stdafx.h"
#include "RAVEn.h"
#include "main/Helper.h"
#include "main/Logger.h"
#include "main/mainworker.h"
#include "main/RFXtrx.h"
#include "tinyxpath/tinyxml.h"
#include "hardware/hardwaretypes.h"

//Rainforest RAVEn USB ZigBee Smart Meter Adapter
//https://rainforestautomation.com/rfa-z106-raven/

RAVEn::RAVEn(const int ID, const std::string& devname)
: device_(devname), m_wptr(m_buffer), m_currUsage(0), m_totalUsage(0)
{
    m_HwdID = ID;
}

bool RAVEn::StartHardware()
{
	RequestStart();

    //Try to open the Serial Port
    try
    {
        Log(LOG_STATUS, "Using serial port: %s", device_.c_str());
        open(device_, 115200);
    }
    catch (boost::exception & e)
    {
        Log(LOG_ERROR, "Error opening serial port!");
#ifdef _DEBUG
        Log(LOG_ERROR, "-----------------\n%s\n-----------------", boost::diagnostic_information(e).c_str());
#else
        (void)e;
#endif
        return false;
    }
    catch (...)
    {
        Log(LOG_ERROR, "Error opening serial port!!!");
        return false;
    }
    setReadCallback([this](auto d, auto l) { readCallback(d, l); });
    m_bIsStarted = true;
    sOnConnected(this);

	StartHeartbeatThread();

    return true;
}

bool RAVEn::StopHardware()
{
    terminate();
    StopHeartbeatThread();
    m_bIsStarted = false;
    return true;
}

void RAVEn::readCallback(const char *indata, size_t inlen)
{
    if (!m_bEnableReceive)
        return; //receiving not enabled

    const char* data;
    for(data = indata; data < indata+inlen; data++)
    {
        if(*data != '\0')
            break;
    }
    if(data == (indata+inlen))
    {
        Log(LOG_ERROR, "RAVEn::readCallback only got NULLs (%d of them)", (int)inlen);
        return;
    }
    size_t len = (indata+inlen) - data;
#ifdef _DEBUG
    Log(LOG_NORM, "RAVEn::readCallback got %d, have %d.  Incoming: %s, Current: %s", len, m_wptr-m_buffer, data, m_buffer);
#endif
    if(m_wptr+len > m_buffer+MAX_BUFFER_LEN)
    {
        Log(LOG_ERROR, "Exceeded buffer space...resetting buffer");
        m_wptr = m_buffer;
    }

    memcpy(m_wptr, data, len);
    m_wptr += len;
    *m_wptr = '\0';

    TiXmlDocument doc;
    const char* endPtr = doc.Parse(m_buffer);
    if (!endPtr)
    {
#ifdef _DEBUG
        Log(LOG_ERROR, "Not enough data received (%d): %s", len, m_buffer);
#endif
        return;
    }
#ifdef _DEBUG
    Log(LOG_NORM, "RAVEn::shifting buffer after parsing %d with %d bytes remaining: %s", endPtr - m_buffer, m_wptr - endPtr, endPtr);
#endif
    memmove(m_buffer, endPtr, m_wptr - endPtr);
    m_wptr = m_buffer + (m_wptr - endPtr);

    TiXmlElement *pRoot;

    pRoot = doc.FirstChildElement("InstantaneousDemand");
    bool updated=false;
    if (pRoot)
    {
	    m_currUsage = 1000 * double(strtoul(pRoot->FirstChildElement("Demand")->GetText(), nullptr, 16))
			  / strtoul(pRoot->FirstChildElement("Divisor")->GetText(), nullptr, 16);
	    updated = true;
    }
    pRoot = doc.FirstChildElement("CurrentSummationDelivered");
    if(pRoot)
    {
	    m_totalUsage = double(strtoul(pRoot->FirstChildElement("SummationDelivered")->GetText(), nullptr, 16))
			   / strtoul(pRoot->FirstChildElement("Divisor")->GetText(), nullptr, 16);
	    updated = true;
    }

    if(updated)
        SendKwhMeter(m_HwdID, 1, 255, m_currUsage, m_totalUsage, "Power Meter");
    else
        Log(LOG_ERROR, "Unknown node");
}

bool RAVEn::WriteToHardware(const char *pdata, const unsigned char length)
{
	return false;
}


