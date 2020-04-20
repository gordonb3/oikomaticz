#set to minimum version that supports clean build
cmake_minimum_required(VERSION 3.14.0)

project(oikomaticz)

# defaults
OPTION(WITH_LINUX_I2C "Include I2C support" YES)
OPTION(WITH_SPI "Include SPI support" YES)
OPTION(WITH_LIBUSB "Enable libusb support" YES)
OPTION(WITH_PYTHON "Use Python for Plugins and Event-Scripts" YES)
OPTION(WITH_OPENZWAVE "Include OpenZwave support" YES)

OPTION(USE_STATIC_BOOST "Build with static BOOST libraries" YES)
OPTION(USE_STATIC_OPENSSL "Build with static OpenSSL libraries" YES)
OPTION(USE_STATIC_LIBSTDCXX "Build with static libgcc/libstdc++ libraries" YES)
OPTION(USE_STATIC_OPENZWAVE "Build with static OpenZwave libraries" YES)


OPTION(WITHOUT_OLDDB_SUPPORT "Disable support for importing an ancient Domoticz database" NO)

## required min. libBoost version
SET(DZ_MIN_LIBBOOST_VERSION 1.66.0)
##

FIND_PROGRAM(GIT_EXECUTABLE git DOC "git command line client")
MACRO(Gitversion_GET_REVISION dir variable)
  EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} --git-dir ./.git rev-list HEAD --count
    WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE ${variable}
    OUTPUT_STRIP_TRAILING_WHITESPACE)
ENDMACRO(Gitversion_GET_REVISION)

Gitversion_GET_REVISION("${CMAKE_SOURCE_DIR}" ProjectRevision)
IF(NOT ProjectRevision)
  MESSAGE(STATUS "Failed to get ProjectRevision from git")
  IF(EXISTS ${CMAKE_SOURCE_DIR}/appversion.default)
    MESSAGE(STATUS "Read ProjectRevision from appversion.default")
    FILE(STRINGS ${CMAKE_SOURCE_DIR}/appversion.default AppVersionContent)
    LIST(GET AppVersionContent 2 AppVersionContent)
    STRING(REPLACE "\"" "" AppVersionContent ${AppVersionContent})
    STRING(REPLACE " " ";" AppVersionContent ${AppVersionContent})
    LIST(GET AppVersionContent 2 ProjectRevision)
  ELSE(EXISTS ${CMAKE_SOURCE_DIR}/appversion.default)
    MESSAGE(STATUS "No appversion.default, set ProjectRevision to 0")
    SET(ProjectRevision 0)
  ENDIF(EXISTS ${CMAKE_SOURCE_DIR}/appversion.default)
ELSE(NOT ProjectRevision)
  MATH(EXPR ProjectRevision "${ProjectRevision}+2107")
ENDIF(NOT ProjectRevision)

MESSAGE(STATUS "###########################")
MESSAGE(STATUS "Compiling Revision #${ProjectRevision}")
MESSAGE(STATUS "###########################")

# The version number.
SET(DZ_VERSION_MAJOR 4)
STRING(SUBSTRING ${ProjectRevision} 0 2 DZ_VERSION_MINOR)
STRING(SUBSTRING ${ProjectRevision} 2 3 DZ_VERSION_PATCH)


IF(WITHOUT_OLDDB_SUPPORT)
  MESSAGE(STATUS "building without olddb support. Minimal required DB version is 129, Domoticz 4.9700 stable")
  ADD_DEFINITIONS(-DNO_PRESTABLE_9700)
ENDIF(WITHOUT_OLDDB_SUPPORT)

IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  SET(CMAKE_CXX_STANDARD 11)
  SET(CMAKE_CXX_STANDARD_REQUIRED ON)
  SET(CXX_EXTENSIONS NO)
ENDIF()

IF(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch -Wno-parentheses-equality -Wno-deprecated-declarations -Wno-tautological-compare -Wno-unused-value -Wno-comment -Wno-unsequenced -Wno-logical-op-parentheses -Wno-literal-conversion")
ENDIF()
IF(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
ENDIF()
IF(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
ENDIF()
IF(CMAKE_COMPILER_IS_GNUCXX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-psabi")
  #SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-switch")
  #ADD_DEFINITIONS( -Wall -O0 -ggdb )
  #ADD_DEFINITIONS( -Wfatal-errors -Wformat=2 -Werror=format-security )
ENDIF()

IF(USE_STATIC_OPENSSL)
  SET(OPENSSL_USE_STATIC_LIBS TRUE)
ENDIF(USE_STATIC_OPENSSL)

IF(WITH_PYTHON)
# FindPythonLibs module is broken for python 3.4+, use pkg_search instead
  #FIND_PACKAGE(PythonInterp 3.4)
  #FIND_PACKAGE(PythonLibs 3.4)
  FIND_PACKAGE(PkgConfig)
  pkg_search_module(PYTHON python>=3.4 python-3.4 python-3.5 python-3.6 python-3.7 python-3.8)
  IF(PYTHON_FOUND)
    MESSAGE(STATUS "found python ${PYTHON_VERSION} includes at: ${PYTHON_INCLUDE_DIRS}")
    INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_DIRS})
    add_definitions(-DENABLE_PYTHON)
  ELSE(PYTHON_FOUND)
    MESSAGE(FATAL_ERROR "Python3 not found on your system, use WITH_PYTHON=NO or install python3-dev)")
  ENDIF(PYTHON_FOUND)
ENDIF(WITH_PYTHON)

SET(EXECUTABLE_OUTPUT_PATH "" CACHE INTERNAL "Where to put the executables for Oikomaticz")

INCLUDE(CheckIncludeFile)
CHECK_INCLUDE_FILE(execinfo.h HAVE_EXECINFO_H)
IF(HAVE_EXECINFO_H)
  # FreeBSD has to include libexecinfo
  IF(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    FIND_LIBRARY(EXECINFO_LIBRARIES NAMES libexecinfo.so)
    IF(EXECINFO_LIBRARIES)
      ADD_DEFINITIONS(-DHAVE_EXECINFO_H)
    ENDIF()
  elseIF(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    FIND_LIBRARY(EXECINFO_LIBRARIES NAMES libexecinfo.so)
    IF(EXECINFO_LIBRARIES)
      ADD_DEFINITIONS(-DHAVE_EXECINFO_H)
    ENDIF()
  ELSE()
    ADD_DEFINITIONS(-DHAVE_EXECINFO_H)
  ENDIF()
ENDIF(HAVE_EXECINFO_H)

INCLUDE(CheckIncludeFiles)
IF(WITH_LINUX_I2C)
  CHECK_INCLUDE_FILES("sys/types.h;linux/i2c-dev.h;linux/i2c.h" HAVE_LINUX_I2C_H)
  IF(HAVE_LINUX_I2C_H)
    MESSAGE(STATUS "Building with I2C support")
    ADD_DEFINITIONS(-DHAVE_LINUX_I2C)
  ELSE(HAVE_LINUX_I2C_H)
    MESSAGE(WARNING "I2C support disabled: headers not found!")
  ENDIF(HAVE_LINUX_I2C_H)
ENDIF(WITH_LINUX_I2C)

IF(WITH_SPI)
  CHECK_INCLUDE_FILES("sys/types.h;linux/spi/spidev.h" HAVE_LINUX_SPI_H)
  IF(HAVE_LINUX_SPI_H)
    MESSAGE(STATUS "Building with SPI support")
    ADD_DEFINITIONS(-DHAVE_LINUX_SPI)
  ELSE(HAVE_LINUX_SPI_H)
    MESSAGE(WARNING "SPI support disabled: headers not found!")
  ENDIF(HAVE_LINUX_SPI_H)
ENDIF(WITH_SPI)

INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(BIGENDIAN)
IF(${BIGENDIAN})
  ADD_DEFINITIONS(-DIS_BIG_ENDIAN)
ENDIF(${BIGENDIAN})

#set(CMAKE_EXE_LINKER_FLAGS "-static")

# a custom target that is always built
ADD_CUSTOM_TARGET(revisiontag ALL)

# creates appversion.h using cmake script
ADD_CUSTOM_COMMAND(TARGET revisiontag COMMAND ${CMAKE_COMMAND}
   -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
   -DMAJORVERSION=${DZ_VERSION_MAJOR}
   -DMINORVERSION=${DZ_VERSION_MINOR}
   -DREVISION=${DZ_VERSION_PATCH}
   -P ${CMAKE_CURRENT_SOURCE_DIR}/getgit.cmake)


## Find libraries  

#
# CURL
#
FIND_PACKAGE(CURL)

#
# Find MD5/RMD160/SHA library
#
FIND_PACKAGE(OpenSSL REQUIRED)

#
# USB
#
IF(WITH_LIBUSB)
  FIND_PATH(LIBUSB_INCLUDE_DIR usb.h
     HINTS ${PC_LIBUSB_INCLUDEDIR} ${PC_LIBUSB_INCLUDE_DIRS})
  FIND_LIBRARY(LIBUSB_LIBRARY NAMES usb
     HINTS ${PC_LIBUSB_LIBDIR} ${PC_LIBUSB_LIBRARY_DIRS})
  SET(LIBUSB_LIBRARIES ${LIBUSB_LIBRARY})

  find_package_handle_standard_args(LIBUSB  DEFAULT_MSG  LIBUSB_LIBRARIES LIBUSB_INCLUDE_DIR)
  IF(NOT LIBUSB_FOUND)
    MESSAGE(STATUS "==== LibUSB not found, support for TE923/Voltcraft disabled!")
  ENDIF(NOT LIBUSB_FOUND)
ENDIF(WITH_LIBUSB)

#
# OpenZWave
# try to find open-zwave, if found, include support
#
IF(WITH_OPENZWAVE)
IF(USE_STATIC_OPENZWAVE)
  FIND_LIBRARY(OpenZWave NAMES libopenzwave.a HINTS "../open-zwave-read-only" "../open-zwave-read-only/cpp/build")
  SET(OPENZWAVE_LIB ${OpenZWave})
ELSE(USE_STATIC_OPENZWAVE)
  pkg_check_modules(OPENZWAVE libopenzwave)
  IF(OPENZWAVE_FOUND)
    MESSAGE(STATUS "==== OpenZWave package found!")
    FIND_LIBRARY(OpenZWave NAMES libopenzwave.so HINTS ${OPENZWAVE_LIBRARY_DIRS})
  ENDIF(OPENZWAVE_FOUND)
ENDIF(USE_STATIC_OPENZWAVE)
ENDIF(WITH_OPENZWAVE)


#
# Tellstick
#
FIND_PATH(TELLDUSCORE_INCLUDE NAMES telldus-core.h)


## Sources

# main include dirs
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/libs)

# source files
ADD_SUBDIRECTORY(src)
GET_DIRECTORY_PROPERTY(DZ_SRCS DIRECTORY src DEFINITION DZ_SRCS)
ADD_EXECUTABLE(oikomaticz ${DZ_SRCS})


# explicitly say that the executable depends on the revisiontag
ADD_DEPENDENCIES(oikomaticz revisiontag)


## Link libraries

#
# Find MD5/RMD160/SHA library
#
IF(NOT OPENSSL_INCLUDE_DIR)
  message(SEND_ERROR "Failed to find OpenSSL include files (ssl.h), no HTTPS support")
ENDIF()
IF(NOT OPENSSL_FOUND)
  message(SEND_ERROR "Failed to find the OpenSSL library, no HTTPS support")
  find_library(MD_LIBRARY NAMES md)
  IF(MD_LIBRARY)
    TARGET_LINK_LIBRARIES(oikomaticz ${MD_LIBRARY})
  ENDIF(MD_LIBRARY)
ELSE()
  message(STATUS "Found OpenSSL ${OPENSSL_VERSION}: ${OPENSSL_LIBRARIES}")
  add_definitions(-DWWW_ENABLE_SSL)
  add_definitions(-DWITH_TLS)
  include_directories(${OPENSSL_INCLUDE_DIR})
  TARGET_LINK_LIBRARIES(oikomaticz ${OPENSSL_LIBRARIES})
ENDIF()

#
# Boost
#
set(Boost_USE_STATIC_LIBS ${USE_STATIC_BOOST})
set(Boost_USE_MULTITHREADED ON)
unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_LIBRARY_DIRS CACHE)

if(USE_STATIC_BOOST)
   message(STATUS "Linking against boost static libraries")
else(USE_STATIC_BOOST)
   message(STATUS "Linking against boost dynamic libraries")
endif(USE_STATIC_BOOST)

find_package(Boost REQUIRED COMPONENTS thread system)
if(Boost_FOUND)
    MESSAGE(STATUS "BOOST libraries found at: ${Boost_LIBRARY_DIRS}")
    MESSAGE(STATUS "Boost includes found at: ${Boost_INCLUDE_DIR}")
else(Boost_FOUND)
    MESSAGE(FATAL_ERROR "Boost thread/system library not found on your system, try to get this installed.")
endif(Boost_FOUND)

target_link_libraries(oikomaticz Boost::thread Boost::system)

# compare found vs required libBoost version
IF(Boost_VERSION VERSION_LESS DZ_MIN_LIBBOOST_VERSION)
   MESSAGE(FATAL_ERROR "Found libBoost version ${Boost_VERSION}, ${DZ_MIN_LIBBOOST_VERSION} or newer required")
ENDIF(Boost_VERSION VERSION_LESS DZ_MIN_LIBBOOST_VERSION)

include_directories(${Boost_INCLUDE_DIRS})

#
# CURL
#
IF(CURL_FOUND)
  MESSAGE(STATUS "Curl ${CURL_VERSION} found at: ${CURL_LIBRARIES}")
  MESSAGE(STATUS "Curl includes found at: ${CURL_INCLUDE_DIRS}")
  INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIRS})
  # ADD_DEFINITIONS(-DWWW_ENABLE_SSL)
ELSE()
  MESSAGE(FATAL_ERROR "cURL not found on your system, see install.txt how to get them installed. (for example 'sudo apt-get install curl libcurl4-gnutls-dev')")
ENDIF(CURL_FOUND)

#
# USB
#
IF(LIBUSB_FOUND)
  MESSAGE(STATUS "Libusb found at: ${LIBUSB_LIBRARIES}")
  add_definitions(-DWITH_LIBUSB)
  TARGET_LINK_LIBRARIES(oikomaticz ${LIBUSB_LIBRARIES})
ENDIF(LIBUSB_FOUND)


#
# OpenZWave
# try to find open-zwave, if found, include support
#
IF(OpenZWave)
  MESSAGE(STATUS "OpenZWave found at: ${OpenZWave}")
  TARGET_LINK_LIBRARIES(oikomaticz ${OpenZWave})
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/libs/openzwave)
#  add_definitions(-DWITH_OPENZWAVE)
ELSE()
  MESSAGE(STATUS "==== OpenZWave not found, support disabled!")
ENDIF(OpenZWave)


IF(EXISTS /sys/class/gpio)
  message(STATUS "GPIO is available")
  add_definitions(-DWITH_GPIO)
ELSE()
  message(STATUS "GPIO is not available")
ENDIF()


IF(TELLDUSCORE_INCLUDE)
  FIND_LIBRARY(TELLDUS_LIBRARIES NAMES libtelldus-core.so)
  IF(TELLDUS_LIBRARIES)
    STRING(REPLACE "/telldus-core.h" "" TELLDUSCORE_PATH ${TELLDUSCORE_INCLUDE})
    MESSAGE(STATUS "Found libtelldus-core at : ${TELLDUSCORE_PATH}, adding telldus support")
    ADD_DEFINITIONS(-DWITH_TELLDUSCORE)
  ELSE(TELLDUS_LIBRARIES)
    MESSAGE(STATUS "Did not find telldus-core, not adding tellstick support")
  ENDIF(TELLDUS_LIBRARIES)
ELSE(TELLDUSCORE_INCLUDE)
  MESSAGE(STATUS "Did not find telldus-core, not adding tellstick support")
ENDIF(TELLDUSCORE_INCLUDE)


# builtin libs
ADD_SUBDIRECTORY(libs)
GET_DIRECTORY_PROPERTY(BUILTIN_INCLUDE_DIRS DIRECTORY libs DEFINITION BUILTIN_INCLUDE_DIRS)
INCLUDE_DIRECTORIES(${BUILTIN_INCLUDE_DIRS})
GET_DIRECTORY_PROPERTY(BUILTIN_LIBRARIES DIRECTORY libs DEFINITION BUILTIN_LIBRARIES)
TARGET_LINK_LIBRARIES(oikomaticz ${BUILTIN_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES} pthread ${CMAKE_DL_LIBS} ${TELLDUS_LIBRARIES})
GET_DIRECTORY_PROPERTY(BUILTIN_DEFINITIONS DIRECTORY libs DEFINITION BUILTIN_DEFINITIONS)
ADD_DEFINITIONS(${BUILTIN_DEFINITIONS})


IF(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  TARGET_LINK_LIBRARIES(oikomaticz )
elseIF(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  TARGET_LINK_LIBRARIES(oikomaticz ${EXECINFO_LIBRARIES})
elseIF(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  TARGET_LINK_LIBRARIES(oikomaticz ${EXECINFO_LIBRARIES})
ELSE()
  TARGET_LINK_LIBRARIES(oikomaticz -lrt -lresolv ${EXECINFO_LIBRARIES})
ENDIF()


# Macro for setting up precompiled headers. Usage:
#
#   add_precompiled_header(target header.h [FORCEINCLUDE])
#
# MSVC: A source file with the same name as the header must exist and
# be included in the target (E.g. header.cpp).
#
# MSVC: Add FORCEINCLUDE to automatically include the precompiled
# header file from every source file.
#
# GCC: The precompiled header is always automatically included from
# every header file.
macro(ADD_PRECOMPILED_HEADER _targetName _input)
  get_filename_component(_inputWe ${_input} NAME_WE)
  set(pch_source ${_inputWe}.cpp)
  foreach(arg ${ARGN})
    if(arg STREQUAL FORCEINCLUDE)
      set(FORCEINCLUDE ON)
    else(arg STREQUAL FORCEINCLUDE)
      set(FORCEINCLUDE OFF)
    endif(arg STREQUAL FORCEINCLUDE)
  endforeach(arg)

  if(MSVC)
    get_target_property(sources ${_targetName} SOURCES)
    set(_sourceFound FALSE)
    foreach(_source ${sources})
      set(PCH_COMPILE_FLAGS "")
      if(_source MATCHES \\.\(cc|cxx|cpp\)$)
        get_filename_component(_sourceWe ${_source} NAME_WE)
        if(_sourceWe STREQUAL ${_inputWe})
          set(PCH_COMPILE_FLAGS "${PCH_COMPILE_FLAGS} /Yc${_input}")
          set(_sourceFound TRUE)
        else(_sourceWe STREQUAL ${_inputWe})
          set(PCH_COMPILE_FLAGS "${PCH_COMPILE_FLAGS} /Yu${_input}")
          if(FORCEINCLUDE)
            set(PCH_COMPILE_FLAGS "${PCH_COMPILE_FLAGS} /FI${_input}")
          endif(FORCEINCLUDE)
        endif(_sourceWe STREQUAL ${_inputWe})
        set_source_files_properties(${_source} PROPERTIES COMPILE_FLAGS "${PCH_COMPILE_FLAGS}")
      endif(_source MATCHES \\.\(cc|cxx|cpp\)$)
    endforeach()
    if(NOT _sourceFound)
      message(FATAL_ERROR "A source file for ${_input} was not found. Required for MSVC builds.")
    endif(NOT _sourceFound)
  endif(MSVC)

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch -Wno-parentheses-equality -Wno-deprecated-declarations -Wno-tautological-compare -Wno-unused-value -Wno-comment -Wno-unsequenced -Wno-logical-op-parentheses -Wno-literal-conversion")
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  endif()
  if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-switch")
    #ADD_DEFINITIONS( -Wall -O0 -ggdb )
    #ADD_DEFINITIONS( -Wfatal-errors -Wformat=2 -Werror=format-security )

    get_filename_component(_name ${_input} NAME)
    set(_source "${CMAKE_CURRENT_SOURCE_DIR}/${_input}")
    set(_outdir "${CMAKE_CURRENT_BINARY_DIR}/${_name}.gch")
    MAKE_DIRECTORY(${_outdir})
    set(_output "${_outdir}/.c++")

    string(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" _flags_var_name)
    # Strip out -Wl,… linker directives as they make GCC fail
    string(REGEX REPLACE " -Wl,[^ ]*" " " _compiler_FLAGS " ${CMAKE_CXX_FLAGS} ${${_flags_var_name}}")

    get_directory_property(_directory_flags INCLUDE_DIRECTORIES)
    foreach(item ${_directory_flags})
      list(APPEND _compiler_FLAGS "-I${item}")
    endforeach(item)

    get_directory_property(_directory_flags COMPILE_DEFINITIONS)
    foreach(item ${_directory_flags})
      list(APPEND _compiler_FLAGS "-D${item}")
    endforeach(item)

    separate_arguments(_compiler_FLAGS)
    message("${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} ${_compiler_FLAGS} -x c++-header -o ${_output} ${_source}")
    add_custom_command(
      OUTPUT ${_output}
      COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} ${_compiler_FLAGS} -x c++-header -o ${_output} ${_source}
      DEPENDS ${_source} )
    add_custom_target(${_targetName}_gch DEPENDS ${_output})
    add_dependencies(${_targetName} ${_targetName}_gch)
    set_target_properties(${_targetName} PROPERTIES COMPILE_FLAGS "-include ${_name} -Winvalid-pch")
  endif(CMAKE_COMPILER_IS_GNUCXX)
endmacro()

option(USE_PRECOMPILED_HEADER "Use precompiled header feature to speed up build time " YES)
if(USE_PRECOMPILED_HEADER)
  message(STATUS "Using precompiled headers")
  ADD_PRECOMPILED_HEADER(oikomaticz "src/stdafx.h")
ENDIF(USE_PRECOMPILED_HEADER)

IF(CMAKE_COMPILER_IS_GNUCXX)
  IF(USE_STATIC_LIBSTDCXX)
    MESSAGE(STATUS "Using static libgcc/libstdc++")
    SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS} -static-libgcc -static-libstdc++")
    SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS} -static-libgcc -static-libstdc++")
    SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -static-libgcc -static-libstdc++")
  ELSE(USE_STATIC_LIBSTDCXX)
    MESSAGE(STATUS "Using dynamic libgcc_s/libstdc++")
  ENDIF(USE_STATIC_LIBSTDCXX)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)


IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  SET(CMAKE_INSTALL_PREFIX "/opt/oikomaticz" CACHE PATH "Install path prefix, prepended onto install directories." FORCE)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

INSTALL(TARGETS oikomaticz DESTINATION ${CMAKE_INSTALL_PREFIX})
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www DESTINATION ${CMAKE_INSTALL_PREFIX})
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/License.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
IF(OpenZWave)
  INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Config DESTINATION ${CMAKE_INSTALL_PREFIX})
ENDIF(OpenZWave)
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dzVents DESTINATION ${CMAKE_INSTALL_PREFIX} PATTERN ".gitignore" EXCLUDE)

SET(CMAKE_INSTALL_DATAPATH "/var/lib/oikomaticz" CACHE PATH "Data path prefix, prepended onto install directories.")
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/History.txt DESTINATION ${CMAKE_INSTALL_DATAPATH})
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts DESTINATION ${CMAKE_INSTALL_DATAPATH} USE_SOURCE_PERMISSIONS)
IF(WITH_PYTHON)
  INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/plugins DESTINATION ${CMAKE_INSTALL_DATAPATH})
ENDIF(WITH_PYTHON)

