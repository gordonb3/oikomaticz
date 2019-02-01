# this macro gets called as a custom build step by running make
# please take into account, that the variable 'SOURCE_DIR' has been defined by the caller

# the git.cmake module is part of the standard distribution
find_package(Git)
if(NOT GIT_FOUND)
  message(FATAL_ERROR "Git not found!.")
endif()

macro(Gitversion_GET_REVISION dir variable)
  execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir ./.git rev-list HEAD --count
    WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE ${variable}
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endmacro(Gitversion_GET_REVISION)

macro(Gitversion_GET_HASH dir variable)
  execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir ./.git rev-parse --short HEAD
    WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE ${variable}
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endmacro(Gitversion_GET_HASH)

macro(Gitversion_GET_DATE dir variable)
  execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir ./.git show -s --format=%ct
    WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE ${variable}
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endmacro(Gitversion_GET_DATE)

macro(Gitversion_CHECK_DIRTY dir variable)
  execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir ./.git diff-index -m --name-only HEAD
    WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE ${variable}
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endmacro(Gitversion_CHECK_DIRTY)

if(NOT REVISION)
  Gitversion_GET_REVISION("${SOURCE_DIR}" ProjectRevision)
  if(NOT ProjectRevision)
    message(STATUS "Failed to get ProjectRevision from git, set it to 0")
    set(ProjectRevision 0)
  else(NOT ProjectRevision)
    math(EXPR ProjectRevision "${ProjectRevision}+2107")
    string(SUBSTRING ${ProjectRevision} 0 2 MINORVERSION)
    string(SUBSTRING ${ProjectRevision} 2 3 REVISION)
  endif(NOT ProjectRevision)
endif()

Gitversion_GET_HASH("${SOURCE_DIR}" ProjectHash)
if(NOT ProjectHash)
  message(STATUS "Failed to get ProjectHash from git, set it to 0")
  set(ProjectHash 0)
endif(NOT ProjectHash)

Gitversion_GET_DATE("${SOURCE_DIR}" ProjectDate)
if(NOT ProjectDate)
  message(STATUS "Failed to get ProjectDate from git, set it to 0")
  set(ProjectDate 0)
endif(NOT ProjectDate)

Gitversion_CHECK_DIRTY("${SOURCE_DIR}" ProjectDirty)
if(ProjectDirty)
  message(STATUS "domoticz has been modified locally: adding \"-modified\" to hash")
  set(ProjectHash "${ProjectHash}-modified")
endif(ProjectDirty)

# write a file with the APPVERSION define
set(FileContent "#pragma once\n")
if(MAJORVERSION)
  string(APPEND FileContent "#define VERSION_STRING \"${MAJORVERSION}.\"\n")
endif(MAJORVERSION)
if(MINORVERSION)
  string(APPEND FileContent "#define APPVERSION \"${MINORVERSION}.${REVISION}\"\n")
else(MINORVERSION)
  message(STATUS "MINORVERSION is not set")
  string(APPEND FileContent "#define APPVERSION ${ProjectRevision}\n")
endif(MINORVERSION)
string(APPEND FileContent "#define APPHASH \"${ProjectHash}\"\n#define APPDATE ${ProjectDate}\n")
file(WRITE ${SOURCE_DIR}/appversion.h.txt ${FileContent})
message(STATUS "writing ${SOURCE_DIR}/appversion.h.txt")

# if ProjectDate is 0, create appversion.h.txt from a copy of appversion.default
if(NOT ProjectDate AND EXISTS ${SOURCE_DIR}/appversion.default)
  message(STATUS "ProjectDate is 0 and appversion.default exists, copy it")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${SOURCE_DIR}/appversion.default ${SOURCE_DIR}/appversion.h.txt)
endif(NOT ProjectDate AND EXISTS ${SOURCE_DIR}/appversion.default)

# copy the file to the final header only if the version changes
# reduces needless rebuilds

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${SOURCE_DIR}/appversion.h.txt ${SOURCE_DIR}/appversion.h)
