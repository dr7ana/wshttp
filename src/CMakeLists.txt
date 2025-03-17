set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

add_library(
    wshttp

    address.cpp
    callbacks.cpp
    dns.cpp
    endpoint.cpp
    format.cpp
    listener.cpp
    loop.cpp
    parser.cpp
    request.cpp
    ssl.cpp
    types.cpp
    utils.cpp
)

target_link_libraries(
    wshttp
    
    PUBLIC
    libevent::core
    libevent::extra
    libevent::ssl
    libevent::threads
    ada::ada
    spdlog::spdlog_header_only
    wshttp_external

    PRIVATE
    wshttp_internal
)

target_compile_features(wshttp INTERFACE cxx_std_20)

if(APPLE)
    target_compile_definitions(wshttp PUBLIC __APPLE_USE_RFC_3542)
endif()
