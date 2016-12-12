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
# Builds psol tarball from a mod_pagespeed checkout.

cd $(dirname "$BASH_SOURCE")/..
source install/build_env.sh || exit 1

buildtype=Release
install_deps=true
run_tests=true
run_packaging=true

eval set -- "$(getopt --long debug,skip_deps,skip_packaging,skip_tests \
                      -o '' -- "$@")"

while [ $# -gt 0 ]; do
  case "$1" in
    --debug) buildtype=Debug; shift; ;;
    --skip_deps) install_deps=false; shift ;;
    --skip_packaging) run_packaging=false; shift; ;;
    --skip_tests) run_tests=false; shift; ;;
    --) shift; break ;;
    *) echo "getopt error" >&2; exit 1 ;;
  esac
done

if [ $# -ne 0 ]; then
  echo "Usage: $(basename $0) [--debug] [--skip_packaging] [--skip_tests]" >&2
  exit 1
fi

if $run_packaging && [ -e psol ] ; then
  echo "A psol/ directory already exists. Move it somewhere else and rerun."
  exit 1
fi

if $install_deps; then
  echo Installing required packages...
  run_with_log log/install_deps.log \
    sudo install/install_required_packages.sh --additional_dev_packages
fi

echo Building PSOL binaries...

MAKE_ARGS=(V=1 BUILDTYPE=$buildtype)

run_with_log log/gyp.log python build/gyp_chromium --depth=.

if $run_tests; then
  run_with_log log/psol_build.log make "${MAKE_ARGS[@]}" \
    mod_pagespeed_test pagespeed_automatic_test
fi

# Using a subshell to contain the cd.
mps_root=$PWD
(cd pagespeed/automatic && \
  run_with_log ../../log/psol_automatic_build.log \
  make "${MAKE_ARGS[@]}" MOD_PAGESPEED_ROOT=$mps_root \
  CXXFLAGS="-DSERF_HTTPS_FETCHING=1" all)

version_h=out/$buildtype/obj/gen/net/instaweb/public/version.h
if [ ! -f $version_h ]; then
  echo "$version_h was not generated!" >&2
  exit 1
fi

source net/instaweb/public/VERSION
build_version="$MAJOR.$MINOR.$BUILD.$PATCH"

if ! grep -q "^#define MOD_PAGESPEED_VERSION_STRING \"$build_version\"$" \
          $version_h; then
  echo "Wrong version found in $version_h" >&2
  exit 1
fi

echo "PSOL built as pagespeed/automatic/pagespeed_automatic.a"
if ! $run_packaging; then
  exit 0
fi

mkdir psol/

if [ "$(uname -m)" = x86_64 ]; then
  bit_size_name=x64
else
  bit_size_name=ia32
fi

bindir="psol/lib/$buildtype/linux/$bit_size_name"
mkdir -p "$bindir"

echo Copying files to psol directory...

cp -f pagespeed/automatic/pagespeed_automatic.a $bindir/
if [ "$buildtype" = "Release" ]; then
  cp -f out/Release/js_minify "$bindir/pagespeed_js_minify"
fi

rsync -arz "." "psol/include/" --prune-empty-dirs \
  --exclude=".svn" \
  --exclude=".git" \
  --include='*.h' \
  --include='*/' \
  --include="apr_thread_compatible_pool.cc" \
  --include="serf_url_async_fetcher.cc" \
  --include="apr_mem_cache.cc" \
  --include="key_value_codec.cc" \
  --include="apr_memcache2.c" \
  --include="loopback_route_fetcher.cc" \
  --include="add_headers_fetcher.cc" \
  --include="console_css_out.cc" \
  --include="console_out.cc" \
  --include="dense_hash_map" \
  --include="dense_hash_set" \
  --include="sparse_hash_map" \
  --include="sparse_hash_set" \
  --include="sparsetable" \
  --include="mod_pagespeed_console_out.cc" \
  --include="mod_pagespeed_console_css_out.cc" \
  --include="mod_pagespeed_console_html_out.cc" \
  --exclude='*'

# Log that we did this.
REPO="$(git config --get remote.origin.url)"
COMMIT="$(git rev-parse HEAD)"

echo "$(date +%F): Copied from mod_pagespeed ${REPO}@${COMMIT} ($USER)" \
  > psol/include_history.txt

echo Creating tarball...
filename_version="${build_version}-${bit_size_name}"
if [ "$buildtype" = "debug" ]; then
  filename_version="${filename_version}-debug"
fi
tar -czf "psol-${filename_version}.tar.gz" psol

echo Cleaning up...
rm -rf psol

echo "PSOL $buildtype build succeeded at $(date)"
