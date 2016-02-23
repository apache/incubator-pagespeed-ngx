# -*- mode: python; -*-
# Copyright 2013 Google Inc.  All rights reserved.
#
# Description:
#   hash_map implementation from rdestl library.  This appears to be more
#   C++-friendly than dense_hash_map and also performs significantly better
#   at default settings.

package(
    default_copts = ["-DDISABLE_GOOGLE_GLOBAL_USING_DECLARATIONS"],
    default_visibility = ["//visibility:public"],
    features = [
        "-layering_check",
        "-parse_headers",
    ],
)

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "rdestl",
    srcs = [
        "allocator.cpp",
    ],
    hdrs = [
        "algorithm.h",
        "alignment.h",
        "allocator.h",
        "functional.h",
        "hash.h",
        "int_to_type.h",
        "iterator.h",
        "rdestl_common.h",
        "rdestl_hash_map.h",
        "type_traits.h",
        "utility.h",
    ],
)
