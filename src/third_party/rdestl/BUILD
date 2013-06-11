# -*- mode: python; -*-
# Copyright 2013 Google Inc.  All rights reserved.
#
# Description:
#   hash_map implementation from rdestl library.  This appears to be more
#   C++-friendly than dense_hash_map and also performs significantly better
#   at default settings.

package(default_visibility = ["//visibility:public"])

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
    includes = [
        # This include-path allows us to consistently include
        # "third_party/rdestl/rdest_hash_map.h" from our code,
        # even though in google3 the path from root is
        # third_party/instaweb/src/third_party/rdestl.  In open
        # source, those top 3 segments are not there.
        "../..",
    ],
    deps = [
    ],
)
