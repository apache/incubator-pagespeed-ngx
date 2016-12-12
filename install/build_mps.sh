#!/bin/bash
#
# Copyright 2016 Google Inc.
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
#
# Author: cheesy@google.com (Steve Hill)
#
# Script to build mod_pagespeed.

source $(dirname "$BASH_SOURCE")/build_env.sh || exit 1

build_type=Release
package_channel=beta
package_type=
log_verbose=
run_tests=true
run_extcache_tests=true

options="$(getopt --long build_deb,build_rpm,debug,release \
  --long skip_extcache_tests,skip_tests,stable_package,verbose \
  -o '' -- "$@")"
eval set -- "$options"

while [ $# -gt 0 ]; do
  case "$1" in
    --build_deb) package_type=deb; shift ;;
    --build_rpm) package_type=rpm; shift ;;
    --debug) build_type=Debug; shift ;;
    --skip_extcache_tests) run_extcache_tests=false; shift ;;
    --skip_tests) run_tests=false; shift ;;
    --stable_package) package_channel=stable; shift ;;
    --verbose) log_verbose=--verbose; shift ;;
    --) shift; break ;;
    *) echo "getopt error" >&2; exit 1 ;;
  esac
done

root="$(git rev-parse --show-toplevel || true)"
[ -n "$root" ] && cd "$root"

if [ ! -d pagespeed -o ! -d third_party ]; then
  echo "Run this from your mod_pagesped client" >&2
  exit 1
fi

MAKE_ARGS=(BUILDTYPE=$build_type)

rm -rf log
mkdir -p log

# TODO(cheesy): The 64-bit build writes artifacts into out/Release not
# out/Release_x64. The fix for that seems to be setting product_dir, see:
# https://groups.google.com/forum/#!topic/gyp-developer/_D7qoTgelaY

run_with_log $log_verbose log/submodule.log \
  git submodule update --init --recursive

run_with_log $log_verbose log/gyp.log python build/gyp_chromium --depth=.

make_targets=(mod_pagespeed)
if $run_tests; then
  make_targets+=(mod_pagespeed_test pagespeed_automatic_test)
  if $run_extcache_tests; then
    make_targets+=(redis_cache_cluster_setup)
  fi
fi

run_with_log $log_verbose log/build.log make \
  "${MAKE_ARGS[@]}" "${make_targets[@]}"

if $run_tests; then
  test_wrapper=
  if $run_extcache_tests; then
    test_wrapper=install/run_program_with_ext_caches.sh
  fi
  BUILDTYPE=$build_type run_with_log $log_verbose log/unit_test.log \
    $test_wrapper out/$build_type/mod_pagespeed_test '&&' \
                  out/$build_type/pagespeed_automatic_test
fi

if [ -n "$package_type" ]; then
  package_target=linux_package_${package_type}_${package_channel}
  MODPAGESPEED_ENABLE_UPDATES=1 run_with_log $log_verbose log/pkg_build.log \
    make "${MAKE_ARGS[@]}" $package_target
fi

echo "$(basename "$0") succeeded at $(date)"
