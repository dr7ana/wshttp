set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

add_library(
    wshttp

    address.cpp
    dns.cpp
    endpoint.cpp
    format.cpp
    listener.cpp
    loop.cpp
    parser.cpp
    request.cpp
    session.cpp
    ssl.cpp
    types.cpp
    utils.cpp
    ws.cpp
)

target_link_libraries(
    wshttp
    
    PUBLIC
    libevent::core
    libevent::extra
    libevent::ssl
    libevent::threads
    ada::ada
    un::log
    wshttp_external

    PRIVATE
    wshttp_internal
)

target_compile_features(wshttp INTERFACE cxx_std_23)

if(APPLE)
    target_compile_definitions(wshttp PUBLIC __APPLE_USE_RFC_3542)
endif()
