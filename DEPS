# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

vars = {
  # chromium.org and googlecode.com will redirect https URLs for directories
  # that don't end with a trailing slash to http. We therefore try to make sure
  # all https URLs include the trailing slash, but it's unclear if SVN actually
  # respects this.
  "chromium_git": "https://chromium.googlesource.com",
  # We don't include @ inside the revision here as is customary since
  # we want to pass this into a -D flag
  "chromium_revision_num": "256281",

  "libpagespeed_svn_root": "https://github.com/pagespeed/page-speed/trunk/",
  "libpagespeed_trunk": "https://github.com/pagespeed/page-speed/trunk/lib/trunk/",
  "libpagespeed_revision": "@2579",

  # Import libwebp 0.5.1 from the official repo.
  "libwebp_src": "https://chromium.googlesource.com/webm/libwebp.git",
  "libwebp_revision": "@v0.5.1",

  "modspdy_src": "https://svn.apache.org/repos/asf/httpd/mod_spdy/trunk",
  "modspdy_revision": "@1661925",

  "serf_src": "https://svn.apache.org/repos/asf/serf/tags/1.3.8/",
  "serf_revision": "@head",

  "apr_src": "https://svn.apache.org/repos/asf/apr/apr/tags/1.5.1/",
  "apr_revision": "@head",

  "aprutil_src": "https://svn.apache.org/repos/asf/apr/apr-util/tags/1.5.4/",
  "aprutil_revision": "@head",

  "apache_httpd_src":
    "https://svn.apache.org/repos/asf/httpd/httpd/tags/2.2.29/",
  "apache_httpd_revision": "@head",

  "apache_httpd24_src":
    "https://svn.apache.org/repos/asf/httpd/httpd/tags/2.4.10/",
  "apache_httpd24_revision": "@head",

  # If the closure library version is updated, the closure compiler version
  # listed in third_party/closure/download.sh should be updated as well.
  "closure_library": "https://github.com/google/closure-library.git",
  "closure_library_revision": "@v20160713",

  "jsoncpp_src": "https://github.com/open-source-parsers/jsoncpp.git",
  "jsoncpp_revision": "@7165f6ac4c482e68475c9e1dac086f9e12fff0d0",

  "gflags_src_revision": "@e7390f9185c75f8d902c05ed7d20bb94eb914d0c",
  "gflags_revision": "@cc7e9a4b374ff7b6a1cae4d76161113ea985b624",

  "google_sparsehash_root":
    "https://github.com/google/sparsehash.git",
  "google_sparsehash_revision": "@6ff8809259d2408cb48ae4fa694e80b15b151af3",

  "gtest_src": "https://github.com/google/googletest.git",
  "gtest_revision": "@c99458533a9b4c743ed51537e25989ea55944908",

  "gmock_src": "https://github.com/google/googlemock.git",
  "gmock_revision": "@c440c8fafc6f60301197720617ce64028e09c79d",

  # Comment this out to disable HTTPS fetching via serf.  See also the
  # references in src/third_party/serf/serf.gyp.
  #
  # TODO(jmarantz): create an easy way to choose this option from the
  # 'gclient' command, without having to edit the gyp & DEPS files.
  # On releases, change this tag from chromium-stable, to the tag of the pinned
  # chromium release at the time of release, eg) "@52.0.2743.116"
  "boringssl_src": "https://boringssl.googlesource.com/boringssl.git",
  "boringssl_git_revision": "@chromium-stable",

  "domain_registry_provider_src":
     "https://github.com/pagespeed/domain-registry-provider.git",
  "domain_registry_provider_revision":
     "@e9b72eaef413335eb054a5982277cb2e42eaead7",

  "libpng_src": "https://github.com/glennrp/libpng.git",
  "libpng_revision": "@a36c4f3f165fb2dd1772603da7f996eb40326621",

  # Brotli v0.5.0-snapshot
  "brotli_src": "https://github.com/google/brotli.git",
  "brotli_revision": "@882f41850b679c1ff4a3804d5515d142a5807376",

  "proto_src": "https://github.com/google/protobuf.git",
  "protobuf_revision": "v3.0.0",

  ## grpc uses nanopb as a git submodule, which gclient doesn't support.
  ## When updating grpc, you should check the nanopb submodule version
  ## specified by your tag, by looking for it under:
  ## https://github.com/grpc/grpc/tree/<TAG>/third_party
  "grpc_src": "https://github.com/grpc/grpc.git",
  "grpc_revision": "v1.0.0",
  "nanopb_src": "https://github.com/nanopb/nanopb.git",
  "nanopb_revision": "f8ac463766281625ad710900479130c7fcb4d63b",

  # When updating Hiredis please ensure that freeReplyObject() is still
  # thread-safe, RedisCache relies on that (see comments in redis_cache.cc).
  # Updates on that matter will probably be tracked here:
  # https://github.com/redis/hiredis/issues/465.
  "hiredis_src": "https://github.com/redis/hiredis.git",
  "hiredis_revision": "v0.13.3",
}

deps = {

  # Other dependencies.
  "src/build/temp_gyp":
    Var("libpagespeed_trunk") + "src/build/temp_gyp/" +
        Var("libpagespeed_revision"),

  # TODO(huibao): Remove references to libpagespeed.
  "src/third_party/giflib":
    Var("libpagespeed_svn_root") + "deps/giflib-4.1.6/",
  "src/third_party/icu": Var("libpagespeed_svn_root") + "deps/icu461/",
  "src/third_party/optipng":
    Var("libpagespeed_svn_root") + "deps/optipng-0.7.4/",
  "src/third_party/zlib": Var("libpagespeed_svn_root") + "deps/zlib-1.2.5/",

  "src/third_party/libjpeg_turbo/yasm/source/patched-yasm":
    Var("chromium_git") + "/chromium/deps/yasm/patched-yasm@7da28c6c7c6a1387217352ce02b31754deb54d2a",
  "src/third_party/libjpeg_turbo/src":
    Var("chromium_git") + "/chromium/deps/libjpeg_turbo/@7260e4d8b8e1e40b17f03fafdf1cd83296900f76",

  "src/testing":
    Var("chromium_git") + "/chromium/src/testing/@3207604f790d18c626e9dcb1a09874618c68844b",
  "src/testing/gtest": Var("gtest_src") + Var("gtest_revision"),
  "src/testing/gmock": Var("gmock_src") + Var("gmock_revision"),


  "src/third_party/apr/src":
    Var("apr_src") + Var("apr_revision"),

  "src/third_party/aprutil/src":
    Var("aprutil_src") + Var("aprutil_revision"),

  "src/third_party/httpd/src/include":
    Var("apache_httpd_src") + "include/" + Var("apache_httpd_revision"),

  "src/third_party/httpd/src/os":
    Var("apache_httpd_src") + "os/" + Var("apache_httpd_revision"),

  "src/third_party/httpd24/src/include":
    Var("apache_httpd24_src") + "include/" + Var("apache_httpd24_revision"),

  "src/third_party/httpd24/src/os":
    Var("apache_httpd24_src") + "os/" + Var("apache_httpd24_revision"),

  "src/third_party/chromium/src/base":
    Var("chromium_git") + "/chromium/src/base@ccf3c2f324c4ae0d1aa878921b7c98f7deca5ee8",

  "src/third_party/chromium/src/build":
    Var("chromium_git") + "/chromium/src/build/@06b7bd9c7a8adb3708db8df4dc058de94f0d5554",

  # This revision is before headers got moved to main chromium repo.
  "src/third_party/chromium/src/googleurl":
    Var("chromium_git") + "/external/google-url@405b6e1798f88e85291820b30344723512e0c38f",

  "src/third_party/closure_library":
    Var("closure_library") + Var("closure_library_revision"),

  "src/third_party/gflags":
    Var('chromium_git') + '/external/webrtc/trunk/third_party/gflags' +
    Var('gflags_revision'),
  "src/third_party/gflags/src":
    Var('chromium_git') + '/external/gflags/src' + Var("gflags_src_revision"),

  "src/third_party/google-sparsehash":
    Var("google_sparsehash_root") + Var("google_sparsehash_revision"),

  "src/third_party/protobuf/src":
    Var("proto_src") + '@' + Var("protobuf_revision"),

  # Json cpp.
  "src/third_party/jsoncpp/src":
    Var("jsoncpp_src") + Var("jsoncpp_revision"),

  # Serf
  "src/third_party/serf/src": Var("serf_src") + Var("serf_revision"),

  "src/third_party/mod_spdy/src": Var("modspdy_src") + Var("modspdy_revision"),

  "src/third_party/libwebp": Var("libwebp_src") + Var("libwebp_revision"),

  "src/tools/clang":
    Var("chromium_git") + "/chromium/src/tools/clang/@bf272f7b05896b9a18de8497383f8b873a86cfbc",

  # This is the same commit as the version from svn included from chromium_deps,
  # but the svn is down, so we take it from chromium-git.
  "src/tools/gyp":
    Var("chromium_git") + "/external/gyp@" + "0fb31294ae844bbf83eba05876b7a241b66f1e99",

  "src/third_party/modp_b64":
    Var("chromium_git") + "/chromium/src/third_party/modp_b64/@aae60754fa997799e8037f5e8ca1f56d58df763d",

  # RE2.
  "src/third_party/re2/src":
    "https://github.com/google/re2.git/@78dd4fa1f86bafbf5a5eb006778d9e6e27297af6",

  # Comment to disable HTTPS fetching via serf.  See also the
  # references in src/third_party/serf/serf.gyp.
  "src/third_party/boringssl/src":
    Var("boringssl_src") + Var("boringssl_git_revision"),

  # Domain Registry Provider gives us the Public Suffix List.
  "src/third_party/domain_registry_provider":
    Var("domain_registry_provider_src") +
        Var("domain_registry_provider_revision"),

  "src/third_party/libpng/src": Var("libpng_src") + Var("libpng_revision"),

  "src/third_party/brotli/src": Var("brotli_src") + Var("brotli_revision"),

  "src/third_party/grpc/src": Var("grpc_src") + '@' + Var("grpc_revision"),

  "src/third_party/grpc/src/third_party/nanopb":
    Var("nanopb_src") + '@' + Var("nanopb_revision"),

  "src/third_party/hiredis/src":
    Var("hiredis_src") + '@' + Var("hiredis_revision"),
}


include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
   "testing",
]


hooks = [
  {
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang,
    # which takes ~20s, but clang speeds up builds by more than 20s.
    "pattern": ".",
    "action": ["python", "src/tools/clang/scripts/update.py", "--mac-only"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "src/build/gyp_chromium",
               "-Dchromium_revision=" + Var("chromium_revision_num")],
  },
  {
    "pattern": ".",
    "action": ["src/third_party/closure/download.sh"],
  },
]
