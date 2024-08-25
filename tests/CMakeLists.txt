add_subdirectory(Catch2)
add_subdirectory(CLI11)

add_library(tests_common STATIC utils.cpp)

target_link_libraries(
    tests_common

    PUBLIC
    wshttp
    wshttp_internal_warnings
    CLI11::CLI11
)
    

add_executable(
    alltests

    001.cpp
    main.cpp
)

target_link_libraries(alltests PRIVATE tests_common Catch2::Catch2)

add_executable(test-client test-client.cpp)
target_link_libraries(test-client PRIVATE tests_common)
