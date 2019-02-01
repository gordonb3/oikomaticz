#set to minimum version that supports clean build on cygwin
cmake_minimum_required(VERSION 2.8.4)

project(oikomaticz_src)


## Target
file(GLOB DZ_core_SRCS main/*.cpp)
file(GLOB DZ_server_SRCS tcpserver/*.cpp webserver/*.cpp)
file(GLOB DZ_messaging_SRCS push/*.cpp notifications/*.cpp smtpclient/*.cpp)

file(GLOB DZ_common_SRCS httpclient/*.cpp)

file(GLOB_RECURSE DZ_devices_SRCS hardware/*.cpp)
list(FILTER DZ_devices_SRCS EXCLUDE REGEX "win")
list(FILTER DZ_devices_SRCS EXCLUDE REGEX "CayenneLPP.cpp")

set(DZ_SRCS ${DZ_core_SRCS} ${DZ_server_SRCS} ${DZ_devices_SRCS} ${DZ_messaging_SRCS} ${DZ_common_SRCS})

