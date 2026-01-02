load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "event",
    cache_entries = {
        "EVENT__LIBRARY_TYPE": "STATIC",
        "EVENT__DISABLE_TESTS": "ON",
        "EVENT__DISABLE_MBEDTLS": "ON",
        "EVENT__DISABLE_BENCHMARK": "ON",
        "EVENT__DISABLE_SAMPLES": "ON",
    },
    lib_source = ":all_srcs",
    out_static_libs = ["libevent.a"],
    visibility = ["//visibility:public"],
)
