# minimum version that supports our build
cmake_minimum_required(VERSION 3.14.0)

project(oikomaticz)

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(TestBigEndian)
include(FindPkgConfig)
include(FindPackageHandleStandardArgs)


### OPTION DEFAULTS
#
#
option(WITH_LINUX_I2C "Include I2C support" NO)
option(WITH_SPI "Include SPI support" NO)
option(WITH_GPIO "Include GPIO support" NO)
option(WITH_LIBUSB "Enable libusb support" NO)
option(WITH_PYTHON "Use Python for Plugins and Event-Scripts" NO)

option(USE_STATIC_BOOST "Build with static BOOST libraries" NO)
option(USE_STATIC_OPENSSL "Build with static OpenSSL libraries" NO)
option(USE_STATIC_LIBSTDCXX "Build with static libgcc/libstdc++ libraries" NO)

option(WITHOUT_OLDDB_SUPPORT "Disable support for importing an ancient Domoticz database" NO)

option(USE_PRECOMPILED_HEADER "Use precompiled header feature to speed up build time " YES)


### DEPENDENCY VERSIONS
#
#
## required min. libBoost version
set(DZ_MIN_LIBBOOST_VERSION 1.66.0)
##


### COMPILER SETTINGS
#
#
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wno-psabi")
  #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-switch")
  #add_definitions( -Wall -O0 -ggdb )
  #add_definitions( -Wfatal-errors -Wformat=2 -Werror=format-security )
  add_definitions( -D_FILE_OFFSET_BITS=64 )
endif()


### VERSIONING
#
#
find_program(GIT_EXECUTABLE git DOC "git command line client")
MACRO(Gitversion_GET_REVISION dir variable)
  execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir ./.git rev-list HEAD --count
    WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE ${variable}
    OUTPUT_STRIP_TRAILING_WHITESPACE)
ENDMACRO(Gitversion_GET_REVISION)

Gitversion_GET_REVISION("${CMAKE_SOURCE_DIR}" ProjectRevision)
if(NOT ProjectRevision)
  message(STATUS "Failed to get ProjectRevision from git")
  if(EXISTS ${CMAKE_SOURCE_DIR}/appversion.default)
    message(STATUS "Read ProjectRevision from appversion.default")
    file(STRINGS ${CMAKE_SOURCE_DIR}/appversion.default AppVersionContent)
    list(GET AppVersionContent 2 AppVersionContent)
    string(REPLACE "\"" "" AppVersionContent ${AppVersionContent})
    string(REPLACE " " ";" AppVersionContent ${AppVersionContent})
    list(GET AppVersionContent 2 ProjectRevision)
  else(EXISTS ${CMAKE_SOURCE_DIR}/appversion.default)
    message(STATUS "No appversion.default, set ProjectRevision to 0")
    set(ProjectRevision 0)
  endif(EXISTS ${CMAKE_SOURCE_DIR}/appversion.default)
else(NOT ProjectRevision)
  math(EXPR ProjectRevision "${ProjectRevision}+2107")
endif(NOT ProjectRevision)

message(STATUS "###########################")
message(STATUS "Compiling Revision #${ProjectRevision}")
message(STATUS "###########################")

# The version number.
set(DZ_VERSION_MAJOR 4)
string(SUBSTRING ${ProjectRevision} 0 2 DZ_VERSION_MINOR)
string(SUBSTRING ${ProjectRevision} 2 3 DZ_VERSION_PATCH)


### FIND OPTIONAL COMPONENTS
#
#

CHECK_INCLUDE_file(execinfo.h HAVE_EXECINFO_H)
if(HAVE_EXECINFO_H)
  # FreeBSD has to include libexecinfo
  if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    find_library(EXECINFO_LIBRARIES NAMES libexecinfo.so)
    if(EXECINFO_LIBRARIES)
      add_definitions(-DHAVE_EXECINFO_H)
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    find_library(EXECINFO_LIBRARIES NAMES libexecinfo.so)
    if(EXECINFO_LIBRARIES)
      add_definitions(-DHAVE_EXECINFO_H)
    endif()
  else()
    add_definitions(-DHAVE_EXECINFO_H)
  endif()
endif(HAVE_EXECINFO_H)

if(WITH_LINUX_I2C)
  CHECK_INCLUDE_FILES("sys/types.h;linux/i2c-dev.h;linux/i2c.h" HAVE_LINUX_I2C_H)
  if(HAVE_LINUX_I2C_H)
    message(STATUS "Building with I2C support")
    add_definitions(-DHAVE_LINUX_I2C)
  else(HAVE_LINUX_I2C_H)
    message(WARNING "I2C support disabled: headers not found!")
  endif(HAVE_LINUX_I2C_H)
endif(WITH_LINUX_I2C)

if(WITH_SPI)
  CHECK_INCLUDE_FILES("sys/types.h;linux/spi/spidev.h" HAVE_LINUX_SPI_H)
  if(HAVE_LINUX_SPI_H)
    message(STATUS "Building with SPI support")
    add_definitions(-DHAVE_LINUX_SPI)
  else(HAVE_LINUX_SPI_H)
    message(WARNING "SPI support disabled: headers not found!")
  endif(HAVE_LINUX_SPI_H)
endif(WITH_SPI)

#
# Python plugin support
#
if(WITH_PYTHON)
# FindPythonLibs module is broken for python 3.4+, use pkg_search instead
  #find_package(PythonInterp 3.4)
  #find_package(PythonLibs 3.4)
  find_package(PkgConfig QUIET)
  pkg_search_module(PYTHON python>=3.4 python-3.4 python-3.5 python-3.6 python-3.7 python-3.8)
  if(PYTHON_FOUND)
    message(STATUS "found python ${PYTHON_VERSION} includes at: ${PYTHON_INCLUDE_DIRS}")
    include_directories(${PYTHON_INCLUDE_DIRS})
    add_definitions(-DENABLE_PYTHON)
  else(PYTHON_FOUND)
    message(FATAL_ERROR "Python3 not found on your system, use WITH_PYTHON=NO or install python3-dev)")
  endif(PYTHON_FOUND)
else(WITH_PYTHON)
    message(STATUS "Building without Python, plugin support disabled")
endif(WITH_PYTHON)


#
# LibUSB support for TE923/Voltcraft
#
if(WITH_LIBUSB)
  find_path(LIBUSB_INCLUDE_DIR usb.h
     HINTS ${PC_LIBUSB_INCLUDEDIR} ${PC_LIBUSB_INCLUDE_DIRS})
  find_library(LIBUSB_LIBRARY NAMES usb
     HINTS ${PC_LIBUSB_LIBDIR} ${PC_LIBUSB_LIBRARY_DIRS})
  set(LIBUSB_LIBRARIES ${LIBUSB_LIBRARY})
  find_package_handle_standard_args(LIBUSB  DEFAULT_MSG  LIBUSB_LIBRARIES LIBUSB_INCLUDE_DIR)
  if(LIBUSB_FOUND)
    message(STATUS "Libusb found at: ${LIBUSB_LIBRARIES}")
    add_definitions(-DWITH_LIBUSB)
  else()
    message(WARNING "==== LibUSB not found, support for TE923/Voltcraft disabled!")
  endif(LIBUSB_FOUND)
else(WITH_LIBUSB)
    message(STATUS "Building without LibUSB, support for TE923/Voltcraft disabled")
endif(WITH_LIBUSB)


#
# Tellstick support
#
find_path(TELLDUSCORE_INCLUDE NAMES telldus-core.h)
if(TELLDUSCORE_INCLUDE)
  find_library(TELLDUS_LIBRARIES NAMES libtelldus-core.so)
endif(TELLDUSCORE_INCLUDE)
if(TELLDUS_LIBRARIES)
  string(REPLACE "/telldus-core.h" "" TELLDUSCORE_PATH ${TELLDUSCORE_INCLUDE})
  message(STATUS "Found libtelldus-core at : ${TELLDUSCORE_PATH}, adding telldus support")
  add_definitions(-DWITH_TELLDUSCORE)
else(TELLDUS_LIBRARIES)
  message(STATUS "Did not find telldus-core, not adding tellstick support")
endif(TELLDUS_LIBRARIES)


#
# GPIO support
#
if(WITH_GPIO)
  if(EXISTS /sys/class/gpio OR FORCE_WITH_GPIO)
    message(STATUS "GPIO is available")
    add_definitions(-DWITH_GPIO)
  else()
    message(WARNING "GPIO is not available")
  endif()
else(WITH_GPIO)
    message(STATUS "Building without GPIO support")
endif(WITH_GPIO)


### SOURCES
#
#

# main include dirs
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/libs)

# source files
add_subdirectory(src)
get_directory_property(DZ_SRCS DIRECTORY src DEFINITION DZ_SRCS)
add_executable(oikomaticz ${DZ_SRCS})
set(EXECUTABLE_OUTPUT_PATH "" CACHE INTERNAL "Where to put the executables for Oikomaticz")


if(WITHOUT_OLDDB_SUPPORT)
  message(STATUS "Building without olddb support. Minimal required DB version is 129, Domoticz 4.9700 stable")
  add_definitions(-DNO_PRESTABLE_9700)
endif(WITHOUT_OLDDB_SUPPORT)


TEST_BIG_ENDIAN(BIGENDIAN)
if(${BIGENDIAN})
  add_definitions(-DIS_BIG_ENDIAN)
endif(${BIGENDIAN})

# a custom target that is always built
add_custom_target(revisiontag ALL)

# creates appversion.h using cmake script
add_custom_command(TARGET revisiontag COMMAND ${CMAKE_COMMAND}
   -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
   -DMAJORVERSION=${DZ_VERSION_MAJOR}
   -DMINORVERSION=${DZ_VERSION_MINOR}
   -DREVISION=${DZ_VERSION_PATCH}
   -P ${CMAKE_CURRENT_SOURCE_DIR}/getgit.cmake)



# explicitly say that the executable depends on the revisiontag
ADD_DEPENDENCIES(oikomaticz revisiontag)


### LINK LIBRARIES
#
#


#
# Find MD5/RMD160/SHA library
#
if(USE_STATIC_OPENSSL)
  set(OPENSSL_USE_STATIC_LIBS TRUE)
endif(USE_STATIC_OPENSSL)

find_package(OpenSSL QUIET REQUIRED)
if(NOT OPENSSL_INCLUDE_DIR)
  message(SEND_ERROR "Failed to find OpenSSL include files (ssl.h), no HTTPS support")
endif()
if(NOT OPENSSL_FOUND)
  message(SEND_ERROR "Failed to find the OpenSSL library, no HTTPS support")
  find_library(MD_LIBRARY NAMES md)
  if(MD_LIBRARY)
    target_link_libraries(oikomaticz ${MD_LIBRARY})
  endif(MD_LIBRARY)
else()
  message(STATUS "Found OpenSSL ${OPENSSL_VERSION}: ${OPENSSL_LIBRARIES}")
  add_definitions(-DWWW_ENABLE_SSL)
  add_definitions(-DWITH_TLS)
  include_directories(${OPENSSL_INCLUDE_DIR})
  target_link_libraries(oikomaticz ${OPENSSL_LIBRARIES})
endif()


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

find_package(Boost QUIET REQUIRED COMPONENTS thread system)
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost thread/system library not found on your system, try to get this installed.")
endif(NOT Boost_FOUND)

# compare found vs required libBoost version
if(Boost_VERSION VERSION_LESS DZ_MIN_LIBBOOST_VERSION)
   message(FATAL_ERROR "Found libBoost version ${Boost_VERSION}, ${DZ_MIN_LIBBOOST_VERSION} or newer required")
else(Boost_VERSION VERSION_LESS DZ_MIN_LIBBOOST_VERSION)
    message(STATUS "Found Boost ${Boost_VERSION} at: ${Boost_INCLUDE_DIR}")
endif(Boost_VERSION VERSION_LESS DZ_MIN_LIBBOOST_VERSION)

target_link_libraries(oikomaticz Boost::thread Boost::system)
include_directories(${Boost_INCLUDE_DIRS})

#
# CURL
#
find_package(CURL QUIET)
if(CURL_FOUND)
  message(STATUS "Curl ${CURL_VERSION_STRING} found at: ${CURL_LIBRARIES}")
  include_directories(${CURL_INCLUDE_DIRS})
else()
  message(FATAL_ERROR "Curl not found on your system, see install.txt how to get them installed. (for example 'sudo apt-get install curl libcurl4-gnutls-dev')")
endif(CURL_FOUND)


# builtin libs
add_subdirectory(libs)
get_directory_property(BUILTIN_INCLUDE_DIRS DIRECTORY libs DEFINITION BUILTIN_INCLUDE_DIRS)
include_directories(${BUILTIN_INCLUDE_DIRS})
get_directory_property(BUILTIN_LIBRARIES DIRECTORY libs DEFINITION BUILTIN_LIBRARIES)
target_link_libraries(oikomaticz ${BUILTIN_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES} pthread ${CMAKE_DL_LIBS} ${TELLDUS_LIBRARIES})
get_directory_property(BUILTIN_DEFINITIONS DIRECTORY libs DEFINITION BUILTIN_DEFINITIONS)
add_definitions(${BUILTIN_DEFINITIONS})



if(LIBUSB_FOUND)
  target_link_libraries(oikomaticz ${LIBUSB_LIBRARIES})
endif(LIBUSB_FOUND)




if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_link_libraries(oikomaticz )
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  target_link_libraries(oikomaticz ${EXECINFO_LIBRARIES})
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  target_link_libraries(oikomaticz ${EXECINFO_LIBRARIES})
else()
  target_link_libraries(oikomaticz -lrt -lresolv ${EXECINFO_LIBRARIES})
endif()


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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-switch")
    #add_definitions( -Wall -O0 -ggdb )
    #add_definitions( -Wfatal-errors -Wformat=2 -Werror=format-security )

    get_filename_component(_name ${_input} NAME)
    set(_source "${CMAKE_CURRENT_SOURCE_DIR}/${_input}")
    set(_outdir "${CMAKE_CURRENT_BINARY_DIR}/${_name}.gch")
    make_directory(${_outdir})
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

if(USE_PRECOMPILED_HEADER)
  message(STATUS "Using precompiled headers")
  ADD_PRECOMPILED_HEADER(oikomaticz "src/stdafx.h")
endif(USE_PRECOMPILED_HEADER)

if(CMAKE_COMPILER_IS_GNUCXX)
  if(USE_STATIC_LIBSTDCXX)
    message(STATUS "Using static libgcc/libstdc++")
    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -static-libgcc -static-libstdc++")
  else(USE_STATIC_LIBSTDCXX)
    message(STATUS "Using dynamic libgcc_s/libstdc++")
  endif(USE_STATIC_LIBSTDCXX)
endif(CMAKE_COMPILER_IS_GNUCXX)


if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/opt/oikomaticz" CACHE PATH "Install path prefix, prepended onto install directories." FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

install(TARGETS oikomaticz DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/License.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dzVents DESTINATION ${CMAKE_INSTALL_PREFIX} PATTERN ".gitignore" EXCLUDE)

set(CMAKE_INSTALL_DATAPATH "/var/lib/oikomaticz" CACHE PATH "Data path prefix, prepended onto install directories.")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/History.txt DESTINATION ${CMAKE_INSTALL_DATAPATH})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts DESTINATION ${CMAKE_INSTALL_DATAPATH} USE_SOURCE_PERMISSIONS)
if(WITH_PYTHON)
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/plugins DESTINATION ${CMAKE_INSTALL_DATAPATH})
endif(WITH_PYTHON)

