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
set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENABLE_LIB_ONLY OFF CACHE BOOL "" FORCE)
add_subdirectory(nghttp2 EXCLUDE_FROM_ALL)

# wslay
set(WSLAY_CONFIGURE_INSTALL OFF CACHE BOOL "" FORCE)
set(WSLAY_STATIC ON CACHE BOOL "" FORCE)
set(WSLAY_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(wslay EXCLUDE_FROM_ALL)
add_library(wslay::wslay ALIAS wslay)

# spdlog
set(SPDLOG_USE_STD_FORMAT OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=0)
add_subdirectory(spdlog)

# libevent
set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
set(EVENT__DISABLE_MBEDTLS ON CACHE BOOL "" FORCE)
set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(libevent)

add_library(libevent::core ALIAS event_core)
add_library(libevent::threads ALIAS event_pthreads)
add_library(libevent::extra ALIAS event_extra)

# ada
set(ADA_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ADA_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(ada EXCLUDE_FROM_ALL)
