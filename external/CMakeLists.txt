include(ExternalProject)

find_package(Git)
if(GIT_FOUND)
    message(STATUS "Checking submodules")
    include(check_submodule)
    check_submodule(ada)
    check_submodule(libevent)
    check_submodule(unlog)
endif()

# unlog
add_subdirectory(unlog)

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
set(ADA_TOOLS OFF CACHE BOOL "" FORCE)
set(ADA_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(ada EXCLUDE_FROM_ALL)
