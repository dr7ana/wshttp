add_library(
    wshttp3

    format.cpp
    server.cpp
)

target_link_libraries(
    wshttp3
    
    PUBLIC
    spdlog::spdlog_header_only
    fmt::fmt
    wslay::wslay
    nghttp2::nghttp2
    wshttp3_external

    PRIVATE
    wshttp3_internal
)
