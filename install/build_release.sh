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

source "$(dirname "$BASH_SOURCE")/build_env.sh" || exit 1

build_debug=false
build_32bit=false
build_psol=true
build_mps_args=(--build_$PKG_EXTENSION)
build_stable=false
verbose=false

options="$(getopt --long 32bit,debug,skip_psol,stable,verbose -o '' -- "$@")"
eval set -- "$options"

while [ $# -gt 0 ]; do
  case "$1" in
    --32bit) build_32bit=true; shift ;;
    --debug) build_debug=true; shift ;;
    --skip_psol) build_psol=false; shift ;;
    --stable) build_stable=true; shift ;;
    --verbose) verbose=true; shift ;;
    --) shift; break ;;
    *) echo "getopt error" >&2; exit 1 ;;
  esac
done

if [ $# -ne 0 ]; then
  echo "Usage: $(basename $0) [--32bit] [--debug] [--stable] [--skip_psol]" >&2
  exit 1
fi

host_is_32bit=true
if [ "$(uname -m)" = x86_64 ]; then
  host_is_32bit=false
fi

if $host_is_32bit && ! $build_32bit; then
  echo "This builds 64-bit binaries by default, but your host is 32-bit" >&2
  echo "If you want a 32-bit binary, try again with --32_bit" >&2
  exit 1
fi

source net/instaweb/public/VERSION
build_version="$MAJOR.$MINOR.$BUILD.$PATCH"

release_dir="release/${build_version}"
if $build_32bit; then
  release_dir="$release_dir/ia32"
else
  release_dir="$release_dir/x64"
fi

if $build_debug; then
  release_dir="${release_dir}_dbg"
fi

rm -rf "$release_dir"
rm -rf log/
mkdir -p log/

# Setup chroot if we need it.

log_verbose=
if $verbose; then
  log_verbose='--verbose'
fi

run_in_chroot=
if $build_32bit && ! $host_is_32bit; then
  run_in_chroot=install/run_in_chroot.sh
  run_with_log $log_verbose log/setup_chroot.log sudo install/setup_chroot.sh
fi

# Run the various build scripts.

if $verbose; then
  build_mps_args+=(--verbose)
fi

if $build_debug; then
  build_mps_args+=(--debug)
fi

if $build_stable; then
  build_mps_args+=(--stable_package)
fi

sudo $run_in_chroot \
  install/run_with_log.sh $log_verbose log/install_packages.log \
    install/install_required_packages.sh --additional_dev_packages
$run_in_chroot install/build_mps.sh "${build_mps_args[@]}"

verbose_flag=
if $verbose; then
  verbose_flag='--verbose'
fi

outdir=out/Release
if $build_debug; then
  outdir=out/Debug
fi
package="$(echo "${outdir}"/mod-pagespeed-*.${PKG_EXTENSION})"

echo "Testing $package"
sudo $run_in_chroot install/test_package.sh $verbose_flag "$package"

if $build_psol; then
  debug=
  if $build_debug; then
    debug='--debug'
  fi
  $run_in_chroot install/build_psol.sh $debug
fi

# Builds all complete, now copy release artifacts into $release_dir.

mkdir -p "$release_dir"
cp "$package" "$release_dir/"

unstripped_suffix=64
if $build_32bit; then
  unstripped_suffix=32
fi

unstripped_suffix="${unstripped_suffix}_${PKG_EXTENSION}"
if [ -n "$build_stable" ]; then
  unstripped_suffix="${unstripped_suffix}_stable"
else
  unstripped_suffix="${unstripped_suffix}_beta"
fi

for lib in libmod_pagespeed libmod_pagespeed_ap24; do
  cp "${outdir}/${lib}.so" \
    "$release_dir/unstripped_${lib}_${unstripped_suffix}.so"
done

if $build_psol; then
  cp psol-*.tar.gz "$release_dir/"
fi

echo "[$(date '+%k:%M:%S')] Build complete"
