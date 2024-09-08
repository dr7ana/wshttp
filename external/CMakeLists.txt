include(ExternalProject)

find_package(Git)
if(GIT_FOUND)
    message(STATUS "Checking submodules")
    include(check_submodule)
    check_submodule(libevent)
    check_submodule(nghttp2)
    check_submodule(spdlog)
    check_submodule(wslay)
endif()

# nghttp2
set(WITH_NEVERBLEED OFF CACHE BOOL "" FORCE)
set(WITH_MRUBY OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(ENABLE_DOC OFF CACHE BOOL "" FORCE)
set(ENABLE_HTTP3 OFF CACHE BOOL "" FORCE)
set(ENABLE_LIB_ONLY ON CACHE BOOL "" FORCE) # toggles off ENABLE_APP and ENABLE_EXAMPLES
add_subdirectory(nghttp2 EXCLUDE_FROM_ALL)

# wslay
set(WSLAY_CONFIGURE_INSTALL OFF CACHE BOOL "" FORCE)
set(WSLAY_STATIC ON CACHE BOOL "" FORCE)
set(WSLAY_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(wslay EXCLUDE_FROM_ALL)
add_library(wslay::wslay ALIAS wslay)

# spdlog
set(SPDLOG_FMT_EXTERNAL_HO ON CACHE BOOL "" FORCE)    # todo: add compile check for fmt version
add_subdirectory(spdlog EXCLUDE_FROM_ALL)

# libevent
set(EVENT__LIBRARY_TYPE "STATIC" CACHE STRING "" FORCE)
set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
set(EVENT__DISABLE_MBEDTLS ON CACHE BOOL "" FORCE)
set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(libevent EXCLUDE_FROM_ALL)

add_library(libevent::ssl ALIAS event_openssl)
add_library(libevent::core ALIAS event_core)
add_library(libevent::threads ALIAS event_pthreads)
add_library(libevent::extra ALIAS event_extra)

# ada
set(ADA_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ADA_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(ada EXCLUDE_FROM_ALL)
