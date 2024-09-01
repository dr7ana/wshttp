add_library(
    wshttp

    address.cpp
    callbacks.cpp
    context.cpp
    dns.cpp
    encoding.cpp
    endpoint.cpp
    format.cpp
    loop.cpp
    parser.cpp
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
    libevent::threads
    ada::ada
    spdlog::spdlog_header_only
    fmt::fmt
    wshttp_external

    PRIVATE
    wshttp_internal
)

target_compile_features(wshttp INTERFACE cxx_std_20)
