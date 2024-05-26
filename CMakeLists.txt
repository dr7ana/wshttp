cmake_minimum_required(VERSION 3.13...3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(LANGS C CXX)
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
	foreach(lang ${LANGS})
    	if(NOT DEFINED CMAKE_${lang}_COMPILER_LAUNCHER AND NOT CMAKE_${lang}_COMPILER MATCHES ".*/ccache")
      		message(STATUS "Enabling ccache for ${lang}")
      		set(CMAKE_${lang}_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "")
    	endif()
  endforeach()
endif()

project(wshttp3
    VERSION 0.0.1
    DESCRIPTION "websocket streaming capable http client"
    LANGUAGES ${LANGS})

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(WSHTTP3_IS_TOPLEVEL_PROJECT TRUE)
else()
    set(WSHTTP3_IS_TOPLEVEL_PROJECT FALSE)
endif()

# TODO: not configured yet for shared vs static
option(BUILD_SHARED_LIBS "Build as shared library" OFF)
option(BUILD_STATIC_DEPS "Build and link against static dependencies" OFF)
option(WARNINGS_AS_ERRORS "Treat all warnings as errors. turn off for development, on for release" OFF)
option(WSHTTP3_BUILD_TESTS "Build wshttp3 test suite" ${WSHTTP3_IS_TOPLEVEL_PROJECT})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

set(default_lto ON)
if(WIN32 OR BUILD_STATIC_DEPS)
    set(default_lto OFF)
endif()
option(WITH_LTO "Compile with Link-time Optimization" ${default_lto})
if(WITH_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_ENABLED OUTPUT ipo_error)
    if(IPO_ENABLED)
        message(STATUS "LTO enabled")
    else()
        message(WARNING "LTO not supported by compiler: ${ipo_error}")
    endif()
else()
    message(STATUS "LTO disabled")
    set(IPO_ENABLED OFF)
endif()

set(IPO_ENABLED OFF)
if(IPO_ENABLED AND NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# external interface target for any general includes meant to be publicly inherited
add_library(wshttp3_external INTERFACE)
target_include_directories(wshttp3_external INTERFACE include)

# internal interface target for flags, paths, etc
add_library(wshttp3_internal INTERFACE)
target_include_directories(wshttp3_internal INTERFACE include/lib/wshttp3)

option(WARNINGS_AS_ERRORS "treat all warnings as errors. turn off for development, on for release" OFF)

set(warning_flags -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-function -Werror=vla -Wdeprecated-declarations)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND warning_flags -Wno-unknown-warning-option)
endif()
if(WARNINGS_AS_ERRORS)
    list(APPEND warning_flags -Werror)
endif()
add_library(wshttp3_internal_warnings INTERFACE)

target_compile_options(wshttp3_internal_warnings INTERFACE "$<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:C>>:${warning_flags}>")
target_link_libraries(wshttp3_internal INTERFACE wshttp3_internal_warnings)

include(FetchContent)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG master
)
FetchContent_MakeAvailable(fmt)

add_subdirectory(external)
add_subdirectory(src)

if(WSHTTP3_BUILD_TESTS)
    add_subdirectory(tests)
endif()
