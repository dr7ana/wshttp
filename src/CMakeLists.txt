add_library(
    wshttp

    format.cpp
    loop.cpp
    server.cpp
)

target_link_libraries(
    wshttp
    
    PUBLIC
    spdlog::spdlog_header_only
    fmt::fmt
    wslay::wslay
    nghttp2::nghttp2
    wshttp_external

    PRIVATE
    wshttp_internal
)
