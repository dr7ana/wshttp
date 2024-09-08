set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

add_library(
    wshttp

    address.cpp
    callbacks.cpp
    context.cpp
    dns.cpp
    format.cpp
    listener.cpp
    endpoint.cpp
    loop.cpp
    node.cpp
    parser.cpp
    request.cpp
    session.cpp
    stream.cpp
    types.cpp
    utils.cpp
)

target_link_libraries(
    wshttp
    
    PUBLIC
    nghttp2::nghttp2
    wslay::wslay
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
