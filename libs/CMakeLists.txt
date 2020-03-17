cmake_minimum_required(VERSION 3.14.0)

project(static_libs)

# defaults
option(USE_BUILTIN_LUA "Use builtin lua library" YES)
option(USE_BUILTIN_MQTT "Use builtin mosquitto library" YES)
option(USE_BUILTIN_SQLITE "Use builtin sqlite library" YES)
option(USE_BUILTIN_ZLIB "Use builtin zlib library" YES)
option(USE_BUILTIN_MINIZIP "Use builtin minizip library" YES)


## static libraries

# jsoncpp
file(GLOB jsoncpp_SRCS jsoncpp/*.cpp)
add_library(jsoncpp STATIC ${jsoncpp_SRCS})

# tinyxpath
file(GLOB tinyxpath_SRCS tinyxpath/*.cpp)
list(FILTER tinyxpath_SRCS EXCLUDE REGEX "tinyxpath/main.cpp")
add_library(tinyxpath STATIC ${tinyxpath_SRCS})

# serial
file(GLOB_RECURSE serial_SRCS serial/*.cpp)
list(FILTER serial_SRCS EXCLUDE REGEX "win")
add_library(serial STATIC ${serial_SRCS})

set(STATIC_LIBRARIES jsoncpp tinyxpath serial)

# openzwave
if(OpenZWave)
  file(GLOB_RECURSE openzwave_SRCS openzwave/*.cpp)
  add_library(openzwave STATIC ${openzwave_SRCS})
  list(APPEND STATIC_LIBRARIES openzwave)
endif()



## included libraries that may be used to override dynamicly linked system provided libraries

#
# LUA
#
if(USE_BUILTIN_LUA)
  message(STATUS "Using builtin LUA library")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_definitions(-DLUA_USE_MACOSX)
  endif()
  add_subdirectory(lua)
  get_directory_property(LUA_LIBRARIES DIRECTORY lua DEFINITION LUA_LIBRARIES)
  set(LUA_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/lua/src")
else(USE_BUILTIN_LUA)
  find_package(PkgConfig)
  pkg_search_module(LUA lua5.2>=5.2 lua>=5.2 lua-5.2)
  if(LUA_FOUND)
    string(SUBSTRING ${LUA_VERSION} 0 3 LUA_MAJOR_MINOR)
    # Lua 5.2 pkg-config is broken.
    # Depending on whether the used distro fixed that to
    # provide backward compatibility we need to find the
    # include dir and library name ourselves.
    if(NOT LUA_INCLUDE_DIRS)
      find_path(LUA_INC_DIRS NAMES "lua.h" PATH_SUFFIXES "lua${LUA_MAJOR_MINOR}")
      set(LUA_INCLUDE_DIRS ${LUA_INC_DIRS})
    endif(NOT LUA_INCLUDE_DIRS)
    set(LUA_DEFINITIONS "-DWITH_EXTERNAL_LUA")
    if(LUA_LIBRARY_DIRS)
      set(LUA_LIBRARIES ${LUA_LIBRARY_DIRS})
    else(LUA_LIBRARY_DIRS)
      find_library(LUA_LIB NAMES lua lua${LUA_MAJOR_MINOR})
      set(LUA_LIBRARIES ${LUA_LIB})
    endif(LUA_LIBRARY_DIRS)
    message(STATUS "Found lua ${LUA_VERSION} at: ${LUA_INCLUDE_DIRS}")
    #string(REGEX REPLACE ".liblua.*so" "" LUA_LIBDIR ${LUA_LIBRARIES})
    #message(STATUS "Found lua library at: ${LUA_LIBDIR}")
    if(LUA_VERSION VERSION_GREATER_EQUAL "5.3")
      list(APPEND LUA_DEFINITIONS "-DWITH_LUA53")
    endif(LUA_VERSION VERSION_GREATER_EQUAL "5.3")
  else(LUA_FOUND)
    message(FATAL_ERROR "LUA not found but USE_BUILTIN_LUA=NO")
  endif(LUA_FOUND)
endif(USE_BUILTIN_LUA)

#
# MOSQUITTO
#
if(USE_BUILTIN_MQTT)
  message(STATUS "Using builtin mosquitto library")
  add_subdirectory(mosquittopp)
  get_directory_property(MQTT_LIBRARIES DIRECTORY mosquittopp DEFINITION MQTT_LIBRARIES)
  add_definitions(-DBUILTIN_MQTT)
  set(MQTT_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/mosquittopp")
else(USE_BUILTIN_MQTT)
  find_path(MQTT_INCLUDE_DIRS NAMES mosquitto.h)
  if(MQTT_INCLUDE_DIRS)
    find_library(MQTT_LIBRARIES NAMES libmosquitto.so)
    if(MQTT_LIBRARIES)
      string(REPLACE "/mosquitto.h" "" MQTT_INCLUDEDIR ${MQTT_INCLUDE_DIRS})
      message(STATUS "Found Mosquitto ${MQTT_VERSION} at: ${MQTT_INCLUDEDIR}")
    else(MQTT_LIBRARIES)
      message(FATAL_ERROR "Mosquitto library not found, but USE_BUILTIN_MQTT=NO")
    endif(MQTT_LIBRARIES)
  else(MQTT_INCLUDE_DIRS)
    message(FATAL_ERROR "Mosquitto not found, but USE_BUILTIN_MQTT=NO")
  endif(MQTT_INCLUDE_DIRS)
endif(USE_BUILTIN_MQTT)

#
# SQLITE
#
if(USE_BUILTIN_SQLITE)
  message(STATUS "Using builtin sqlite library")
  add_subdirectory(sqlite)
  get_directory_property(SQLite_LIBRARIES DIRECTORY sqlite DEFINITION SQLite_LIBRARIES)
  set(SQLite_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/sqlite")
else(USE_BUILTIN_SQLITE)
  find_package(PkgConfig)
  pkg_check_modules(SQLite QUIET sqlite3)
  if(SQLite_FOUND)
    set(SQLite_DEFINITIONS "-DWITH_EXTERNAL_SQLITE")
    message(STATUS "Found SQLite ${SQLite_VERSION} at: ${SQLite_INCLUDEDIR}")
    #message(STATUS "Found SQLite3 libraries at: ${SQLITE_LIBDIR}")
  else(SQLite_FOUND)
    message(FATAL_ERROR "SQLite3 not found but USE_BUILTIN_SQLITE=NO")
  endif(SQLite_FOUND)
endif(USE_BUILTIN_SQLITE)

#
# ZLIB
#
if(USE_BUILTIN_ZLIB)
  message(STATUS "Using builtin zlib library")
  set(ZLIB_INCLUDE_DIRS zlib)
  set(ZLIB_LIBRARIES z)
  add_subdirectory(${ZLIB_INCLUDE_DIRS})
  add_library(z STATIC IMPORTED)
  set_target_properties(z PROPERTIES IMPORTED_LOCATION ${ZLIB_INCLUDE_DIRS}/libz.a)
else(USE_BUILTIN_ZLIB)
  find_package(ZLIB REQUIRED)
  if(ZLIB_FOUND)
    message(STATUS "Found Zlib ${ZLIB_VERSION} at: ${ZLIB_INCLUDE_DIRS}")
    #string(REPLACE "/libz.so" "" ZLIB_LIBDIR ${ZLIB_LIBRARIES})
    #message(STATUS "Found Zlib libraries at: ${ZLIB_LIBDIR}")
  else()
    message(FATAL_ERROR "Zlib not found but USE_BUILTIN_ZLIB=NO")
  endif(ZLIB_FOUND)
endif(USE_BUILTIN_ZLIB)

#
# MINIZIP
#
if(USE_BUILTIN_MINIZIP)
  message(STATUS "Using builtin minizip library")
  set(MINIZIP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/zlib/contrib/minizip")
  set(MINIZIP_LIBRARIES minizip)
  set(minizip_SRCS zlib/contrib/minizip/unzip.c zlib/contrib/minizip/ioapi.c)
  add_library(minizip STATIC ${minizip_SRCS})
else(USE_BUILTIN_MINIZIP)
  find_package(PkgConfig)
  pkg_check_modules(MINIZIP QUIET minizip)
  if(MINIZIP_FOUND)
    message(STATUS "Found Minizip ${MINIZIP_VERSION} at: ${MINIZIP_INCLUDE_DIRS}")
    #message(STATUS "Found Minizip libraries at: ${MINIZIP_LIBDIR}")
  else()
    message(FATAL_ERROR "Minizip not found but USE_BUILTIN_MINIZIP=NO")
  endif(MINIZIP_FOUND)
endif(USE_BUILTIN_MINIZIP)


## combine paths for querying by our main project file
set(BUILTIN_LIBRARIES ${STATIC_LIBRARIES} ${LUA_LIBRARIES} ${MQTT_LIBRARIES} ${SQLite_LIBRARIES} ${ZLIB_LIBRARIES} ${MINIZIP_LIBRARIES})
set(BUILTIN_INCLUDE_DIRS ${LUA_INCLUDE_DIRS} ${MQTT_INCLUDE_DIRS} ${SQLite_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${MINIZIP_INCLUDE_DIRS})
set(BUILTIN_DEFINITIONS ${LUA_DEFINITIONS} ${SQLite_DEFINITIONS})

