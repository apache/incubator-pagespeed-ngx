#!/bin/bash
#
# Copyright 2013 Google Inc.
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
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs ngx_pagespeed system tests.
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.
#
# Usage:
#   ./run_tests.sh /path/to/mod_pagespeed /path/to/nginx/binary
#
# By default the test script uses several ports.  If you have a port conflict
# and need to override one you can do that by setting the relevant environment
# variable.  For example:
#   PRIMARY_PORT=1234 ./run_tests.sh /.../mod_pagespeed /.../nginx/binary

# Normally we test only with the native fetcher off.  Set
# TEST_NATIVE_FETCHER=true to also test the native fetcher, set
# TEST_SERF_FETCHER=false to skip the serf fetcher.
TEST_NATIVE_FETCHER=${TEST_NATIVE_FETCHER:-false}
TEST_SERF_FETCHER=${TEST_SERF_FETCHER:-true}

# Normally we actually run the tests, but you might only want us to set up nginx
# for you so you can do manual testing.  If so, set RUN_TESTS=false and this
# will exit after configuring and starting nginx.
RUN_TESTS=${RUN_TESTS:-true}

# To run tests with valgrind, set the environment variable USE_VALGRIND to
# true.
USE_VALGRIND=${USE_VALGRIND:-false}

if [ "$#" -ne 2 ] ; then
  echo "Usage: $0 mod_pagespeed_dir nginx_executable"
  exit 2
fi

MOD_PAGESPEED_DIR="$1"
NGINX_EXECUTABLE="$2"

: ${PRIMARY_PORT:=8050}
: ${SECONDARY_PORT:=8051}
: ${CONTROLLER_PORT:=8053}
: ${RCPORT:=9991}
: ${PAGESPEED_TEST_HOST:=selfsigned.modpagespeed.com}

this_dir="$( cd $(dirname "$0") && pwd)"

function run_test_checking_failure() {
  USE_VALGRIND="$USE_VALGRIND" \
    PRIMARY_PORT="$PRIMARY_PORT" \
    SECONDARY_PORT="$SECONDARY_PORT" \
    MOD_PAGESPEED_DIR="$MOD_PAGESPEED_DIR" \
    NGINX_EXECUTABLE="$NGINX_EXECUTABLE" \
    PAGESPEED_TEST_HOST="$PAGESPEED_TEST_HOST" \
    RUN_TESTS="$RUN_TESTS" \
    CONTROLLER_PORT="$CONTROLLER_PORT" \
    RCPORT="$RCPORT" \
    bash "$this_dir/nginx_system_test.sh"
  STATUS=$?
  echo "With $@ setup."
  case $STATUS in
    0)
      return  # No failure.
      ;;
    3)
      return  # Only expected failures.
      ;;
    4)
      return  # Return passing error code when running manually.
      ;;
    *)
      exit 1  # Real failure.
  esac
}

if $TEST_SERF_FETCHER; then
  NATIVE_FETCHER=off run_test_checking_failure "serf fetcher"
fi

if $TEST_NATIVE_FETCHER; then
  NATIVE_FETCHER=on run_test_checking_failure "native fetcher"
fi
