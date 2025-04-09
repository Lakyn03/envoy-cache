load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_extension",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

envoy_extension_package()

envoy_cc_library(
    name = "my_cache_filter_lib",
    srcs = ["my_cache_filter.cc"],
    hdrs = ["my_cache_filter.h"],
    deps = [
        "//envoy/http:filter_interface",
        "//source/extensions/filters/http/common:pass_through_filter_lib",
        "@envoy_api//envoy/extensions/filters/http/my_cache/v3:pkg_cc_proto",
    ],
)

envoy_cc_extension(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        "//envoy/registry",
        "//envoy/server:filter_config_interface",
        "//source/common/protobuf:utility_lib",
        "//source/extensions/filters/http/common:factory_base_lib",
        "//source/extensions/filters/http/my_cache:my_cache_filter_lib",
        "@envoy_api//envoy/extensions/filters/http/my_cache/v3:pkg_cc_proto",
    ],
)
