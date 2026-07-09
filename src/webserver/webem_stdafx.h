#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#endif

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <algorithm>
