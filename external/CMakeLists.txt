include(ExternalProject)

find_package(Git)
if(GIT_FOUND)
    message(STATUS "Checking submodules")
    include(check_submodule)
    check_submodule(ada)
    check_submodule(uneventful)
endif()

# uneventful
add_subdirectory(uneventful)

# ada
set(ADA_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(ADA_TOOLS OFF CACHE BOOL "" FORCE)
set(ADA_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(ada EXCLUDE_FROM_ALL)
