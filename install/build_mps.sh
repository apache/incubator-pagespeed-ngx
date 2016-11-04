#!/bin/bash

source $(dirname "$BASH_SOURCE")/build_env.sh || exit 1

build_type=Release
package_channel=beta
package_type=
log_verbose=
run_tests=true

options="$(getopt --long \
  build_deb,build_rpm,debug,release,skip_tests,stable_package,verbose \
  -o '' -- "$@")"
eval set -- "$options"

while [ $# -gt 0 ]; do
  case "$1" in
    --build_deb) package_type=deb; shift ;;
    --build_rpm) package_type=rpm; shift ;;
    --debug) build_type=Debug; shift ;;
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

# TODO(cheesy): gclient will be going away soon.
if [ -d depot_tools ]; then
  (cd depot_tools && git pull)
else
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
fi

PATH="$PATH:$PWD/depot_tools"

rm -rf log
mkdir -p log

# TODO(cheesy): The 64-bit build writes artifacts into out/Release not
# out/Release_x64. The fix for that seems to be setting product_dir, see:
# https://groups.google.com/forum/#!topic/gyp-developer/_D7qoTgelaY

run_with_log $log_verbose log/gclient.log gclient config \
  https://github.com/pagespeed/mod_pagespeed.git --unmanaged --name="$PWD"
run_with_log $log_verbose log/gclient.log gclient sync --force

make_targets=(mod_pagespeed)
if $run_tests; then
  make_targets+=(mod_pagespeed_test pagespeed_automatic_test)
fi

run_with_log $log_verbose log/build.log make \
  "${MAKE_ARGS[@]}" "${make_targets[@]}"

if $run_tests; then
  run_with_log $log_verbose log/unit_test.log \
    out/Release/mod_pagespeed_test
  run_with_log $log_verbose log/unit_test.log \
    out/Release/pagespeed_automatic_test
fi

if [ -n "$package_type" ]; then
  package_target=linux_package_${package_type}_${package_channel}
  MODPAGESPEED_ENABLE_UPDATES=1 run_with_log $log_verbose build.log \
    make "${MAKE_ARGS[@]}" $package_target
fi

echo "Build succeeded at $(date)"
