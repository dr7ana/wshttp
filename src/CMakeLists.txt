add_library(
    wshttp

    callbacks.cpp
    context.cpp
    dns.cpp
    endpoint.cpp
    format.cpp
    loop.cpp
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
    spdlog::spdlog_header_only
    fmt::fmt
    wshttp_external

    PRIVATE
    wshttp_internal
)

target_compile_features(wshttp INTERFACE cxx_std_20)
