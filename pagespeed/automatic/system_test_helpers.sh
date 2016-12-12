#!/bin/bash
#
# Copyright 2012 Google Inc.
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
# Set up variables and functions for use by various system tests.
#
# Scripts using this file (callers) should 'source' or '.' it so that an error
# detected in a function here can exit the caller.  Callers should preface tests
# with:
#   start_test <test name>
# A test should use check, check_from, fetch_until, and other functions defined
# below, as appropriate.  A test should not directly call exit on failure.
#
# Callers should leave argument parsing to this script.
#
# Callers should invoke check_failures_and_exit after no more tests are left
# so that expected failures can be logged.
#
# If command line args are wrong, exit with status code 2.
# If no tests fail, it will exit the shell-script with status 0.
# If a test fails:
#  - If it's listed in PAGESPEED_EXPECTED_FAILURES or CONTINUE_AFTER_FAILURE is
#    "true", log the name of the failing test to display when
#    check_failures_and_exit is called, at which point exit with status code 3
#    if the failure was expected or 1 otherwise.
#  - Otherwise, exit immediately with status code 1.
# TODO(jefftk): After all tests are converted to use run_test, rework expected
#               failures so that it applies to run_test names and not start_test
#               names.
#
# The format of PAGESPEED_EXPECTED_FAILURES is '~' separated test names.
# For example:
#   PAGESPEED_EXPECTED_FAILURES="convert_meta_tags~extend_cache"
# or:
#    PAGESPEED_EXPECTED_FAILURES="
#       ~compression is enabled for rewritten JS.~
#       ~convert_meta_tags~
#       ~regression test with same filtered input twice in combination"
#
# Callers need to set SERVER_NAME, and not run this more than once
# simultaneously with the same SERVER_NAME value.

set -u  # Disallow referencing undefined variables.

# Catch potential misuse of this script.
if [ "$(basename $0)" == "system_test_helpers.sh" ] ; then
  echo "ERROR: This file must be loaded with source."
  exit 2
fi

if [ $# -lt 1 -o $# -gt 3 ]; then
  # Note: HOSTNAME and HTTPS_HOST should generally be localhost (when using
  # the default port) or localhost:PORT (when not). Specifically, by default
  # /mod_pagespeed_statistics is only accessible when accessed as localhost.
  echo Usage: $(basename $0) HOSTNAME [HTTPS_HOST [PROXY_HOST]]
  exit 2
fi;

if [ -z "${TEMPDIR:-}" ]; then
  TEMPDIR="/tmp/mod_pagespeed_test.$USER/$SERVER_NAME"
  # If someone else is supplying a TEMPDIR then it's their responsibility to
  # make sure it's clean, but if we're using the default one then we need to
  # clean it up on start so settings from previous tests don't affect this one.
  # Cleaning up on exit doesn't work because if there's a test failure we want
  # to leave things as they are to help with debugging.
  #
  # Because TEMPDIR includes SERVER_NAME this still allows, for example,
  # parallel Apache and Nginx test execution.
  rm -rf "$TEMPDIR"
  mkdir -p "$TEMPDIR"
fi

# EXPECTED_FAILURES acts on "start_test" tests, while UNEXPECTED_FAILURES acts
# on "run_test" tests.
# TODO(jefftk): after we've converted everything to use run_test, including
# nginx_system_test.sh, switch EXPECTED_FAILURES to work on run_test instead.
EXPECTED_FAILURES="${TEMPDIR}/expected_failures"
UNEXPECTED_FAILURES="${TEMPDIR}/unexpected_failures"

# Make this easier to process so we're always looking for '~target~'.
PAGESPEED_EXPECTED_FAILURES="~${PAGESPEED_EXPECTED_FAILURES=}~"

# If the user has specified an alternate WGET as an environment variable, then
# use that, otherwise use the one in the path.
# Note: ${WGET:-} syntax is used to avoid breaking "set -u".
if [ "${WGET:-}" == "" ]; then
  WGET=wget
else
  echo WGET = $WGET
fi

if ! $WGET --version | head -1 | grep -q "1\.1[2-9]"; then
  echo "You have the wrong version of wget. >1.12 is required."
  exit 1
fi

# Ditto for curl.
if [ "${CURL:-}" == "" ]; then
  CURL=curl
else
  echo CURL = $CURL
fi

# Note that 'curl --version' exits with status 2 on CentOS even when
# curl is installed.
if ! which $CURL > /dev/null 2>&1; then
  echo "curl ($CURL) is not installed."
  exit 1
fi

# We need to set a wgetrc file because of the stupid way that the bash deals
# with strings and variable expansion.
mkdir -p $TEMPDIR || exit 1
export WGETRC=$TEMPDIR/wgetrc
# Use a Chrome User-Agent, so that we get real responses (including compression)
cat > $WGETRC <<EOF
user_agent = Mozilla/5.0 (X11; U; Linux x86_64; en-US) AppleWebKit/534.0 (KHTML, like Gecko) Chrome/6.0.408.1 Safari/534.0
EOF

# You can pass in TEST_TO_RUN=test-name to run only a specific test.  This is
# intended for debugging, where you want to iterate on a single failing test.
# It only works with tests that are set up to use run_test, which is currently
# only the ones in automatic/.  Tests that haven't been converted to use
# run_test currently always run.
# TODO(jefftk): convert all system tests to use run_test and separate files.
TEST_TO_RUN="${TEST_TO_RUN:-}"

# Individual tests should use $TESTTMP if they need to store something
# temporarily.  Infrastructure can use $ORIGINAL_TEMPDIR if it's ok with
# parallel use.
TESTTMP="$TEMPDIR"
ORIGINAL_TEMPDIR="$TEMPDIR"
unset TEMPDIR

HELPERS_LOADED=1
HOSTNAME=$1
PRIMARY_SERVER=http://$HOSTNAME
EXAMPLE_ROOT=$PRIMARY_SERVER/mod_pagespeed_example
# TODO(sligocki): Should we be rewriting the statistics page by default?
# Currently we are, so disable that so that it doesn't spoil our stats.
DEFAULT_STATISTICS_URL=$PRIMARY_SERVER/mod_pagespeed_statistics?PageSpeed=off
STATISTICS_URL=${STATISTICS_URL:-$DEFAULT_STATISTICS_URL}
DEFAULT_GLOBAL_STATISTICS_URL="$PRIMARY_SERVER/pagespeed_global_admin/statistics?PageSpeed=off"
GLOBAL_STATISTICS_URL=${GLOBAL_STATISTICS_URL:-$DEFAULT_GLOBAL_STATISTICS_URL}
BAD_RESOURCE_URL=$PRIMARY_SERVER/mod_pagespeed/W.bad.pagespeed.cf.hash.css
MESSAGE_URL=$PRIMARY_SERVER/pagespeed_admin/message_history
CONSOLE_URL=$PRIMARY_SERVER/pagespeed_admin/console

# In some servers (Nginx) PageSpeed process html after headers are finalized,
# while in others (Apache) it runs before and has to treat them as tentative.
HEADERS_FINALIZED=${HEADERS_FINALIZED:-true}

# The following shake-and-bake ensures that we set REWRITTEN_TEST_ROOT based on
# the TEST_ROOT in effect when we start up, if any, but if it was not set before
# invocation it is set to the newly-chosen TEST_ROOT.  This permits us to call
# this from other test scripts that use different host prefixes for rewritten
# content.
REWRITTEN_TEST_ROOT=${TEST_ROOT:-}
TEST_ROOT=$PRIMARY_SERVER/mod_pagespeed_test
REWRITTEN_TEST_ROOT=${REWRITTEN_TEST_ROOT:-$TEST_ROOT}

# This sets up similar naming for https requests.
HTTPS_HOST=${2:-}
HTTPS_EXAMPLE_ROOT=https://$HTTPS_HOST/mod_pagespeed_example


# Determines whether a variable is defined, even with set -u
#   http://stackoverflow.com/questions/228544/
#   how-to-tell-if-a-string-is-not-defined-in-a-bash-shell-script
# albeit there are zero votes for that answer.
function var_defined() {
  local var_name=$1
  set | grep "^${var_name}=" 1>/dev/null
  return $?
}

# These are the root URLs for rewritten resources; by default, no change.
REWRITTEN_ROOT=${REWRITTEN_ROOT:-$EXAMPLE_ROOT}
if ! var_defined PROXY_DOMAIN; then
  PROXY_DOMAIN="$HOSTNAME"
fi

# Setup wget proxy information
if [ -n "${3:-}" ]; then
  export http_proxy=http://$3
else
  export http_proxy=""
fi
export https_proxy=${http_proxy}
export ftp_proxy=${http_proxy}
export no_proxy=""

# Version timestamped with nanoseconds, making it extremely unlikely to hit.
BAD_RND_RESOURCE_URL="$PRIMARY_SERVER/mod_pagespeed/bad$(date +%N).\
pagespeed.cf.hash.css"

combine_css_filename=\
styles/yellow.css+blue.css+big.css+bold.css.pagespeed.cc.xo4He3_gYf.css

OUTDIR=$TESTTMP/fetched_directory
rm -rf $OUTDIR
mkdir -p $OUTDIR

# Run a single test, and exit if it does.  Use this for tests that are fast and
# should never flake, in order to make sure the system is up before we start
# continuing after failures.
function run_critical_test() {
  run_test_helper "$@"
}

function run_test() {
  if ! run_test_helper "$@"; then
    if "${CONTINUE_AFTER_FAILURE:-false}"; then
      echo "$@" >> "$UNEXPECTED_FAILURES"
    else
      exit 1
    fi
  fi
}

# Individual tests are in separate files under system_tests/ and may be run
# individually or reordered.  If one test must be run after another, put them in
# the same file.
SYSTEM_TEST_DIR="DEFINE_THIS_BEFORE_USING_RUN_TEST"
function run_test_helper() {
  local test_name=$1

  if [ -n "$TEST_TO_RUN" ] && [ "$TEST_TO_RUN" != "$test_name" ]; then
    return  # By default TEST_TO_RUN="" so normally we don't skip tests here.
  fi

  # Use a subshell to keep modifications tests make to the test environment
  # from interfering with eachother.
  previous_time_ms=0
  if ! (source "$SYSTEM_TEST_DIR/${test_name}.sh"); then
    return 1
  fi
  update_elapsed_time
}

# This function expects to be run in the background and then killed when we know
# how the test finished.
function tail_while_waiting() {
  local test_name="$1"
  local test_log="$2"

  # In case it's already done or nearly done, don't print anything.
  sleep 1
  echo "Still waiting for $test_name"
  echo "tail -f $test_log"
  tail -f "$test_log"
}

# Returns the unix system time in milliseconds.
function now_ms() {
  # Note: the '%N' probably won't work on FreeBSD, and another solution will be
  # needed to the current time in milliseconds.
  date +%s%N | cut -b1-13
}

# Prints the elapsed time since the last time update_elapsed_time was called.
previous_time_ms=0
function update_elapsed_time() {
  current_time_ms=$(now_ms)
  if [ "$previous_time_ms" != 0 ]; then
    echo 'ELAPSED TIME:' $((current_time_ms - previous_time_ms))"ms"
  fi
  previous_time_ms="$current_time_ms"
}

CURRENT_TEST="pre tests"
function start_test() {
  update_elapsed_time
  WGET_ARGS=""
  CURRENT_TEST="$@"
  echo "TEST: $CURRENT_TEST"
}

# Wget is used three different ways.  The first way is nonrecursive and dumps a
# single page (with headers) to standard out.  This is useful for grepping for a
# single expected string that's the result of a first-pass rewrite:
#   wget -q -O --save-headers - $URL | grep -q foo
# "-q" quells wget's noisy output; "-O -" dumps to stdout; grep's -q quells
# its output and uses the return value to indicate whether the string was
# found.  Note that exiting with a nonzero value will immediately kill
# the make run.
#
# Sometimes we want to check for a condition that's not true on the first dump
# of a page, but becomes true after a few seconds as the server's asynchronous
# fetches complete.  For this we use the the fetch_until() function:
#   fetch_until $URL 'grep -c delayed_foo' 1
# In this case we will continuously fetch $URL and pipe the output to
# grep -c (which prints the count of matches); we repeat until the number is 1.
#
# The final way we use wget is in a recursive mode to download all prerequisites
# of a page.  This fetches all resources associated with the page, and thereby
# validates the resources generated by mod_pagespeed:
#   wget -H -p -S -o $WGET_OUTPUT -nd -P $WGET_DIR $EXAMPLE_ROOT/$FILE
# Here -H allows wget to cross hosts (e.g. in the case of a sharded domain); -p
# means to fetch all prerequisites; "-S -o $WGET_OUTPUT" saves wget output
# (including server headers) for later analysis; -nd puts all results in one
# directory; -P specifies that directory.  We can then run commands on
# $WGET_DIR/$FILE and nuke $WGET_DIR when we're done.
# TODO(abliss): some of these will fail on windows where wget escapes saved
# filenames differently.
# TODO(morlovich): This isn't actually true, since we never pass in -r,
#                  so this fetch isn't recursive. Clean this up.

function define_fetch_variables {
  WGET_OUTPUT=$OUTDIR/wget_output.txt
  # We use a separate directory so that it can be rm'd without disturbing other
  # data in $OUTDIR.
  WGET_DIR=$OUTDIR/wget
  WGET_DUMP="$WGET -q -O - --save-headers"
  WGET_DUMP_HTTPS="$WGET -q -O - --save-headers --no-check-certificate"
  PREREQ_ARGS="-H -p -S -o $WGET_OUTPUT -nd -P $WGET_DIR/ -e robots=off"
  WGET_PREREQ="$WGET $PREREQ_ARGS"
  WGET_ARGS=""
}
define_fetch_variables

function run_wget_with_args() {
  echo $WGET_PREREQ $WGET_ARGS "$@"
  $WGET_PREREQ $WGET_ARGS "$@"
}

# Should be called at the end of any system test using this script.  While most
# errors will be reported immediately and will make us exit with status 1, tests
# listed in PAGESPEED_EXPECTED_FAILURES will let us continue.  This prints out
# failure information for these tests, if appropriate.
#
# This function always exits the script:
#   Status 0: pass
#   Status 1: fail
#   Status 3: only expected failures
function check_failures_and_exit() {
  update_elapsed_time
  if [ -e "$UNEXPECTED_FAILURES" ] ; then
    echo "Failing Tests:"
    sed 's/^/  /' "$UNEXPECTED_FAILURES"
    echo "FAIL."
    exit 1
  elif [ -e "$EXPECTED_FAILURES" ] ; then
    echo Expected Failing Tests:
    sed 's/^/  /' "$EXPECTED_FAILURES"
    echo "MOSTLY PASS.  Expected failures only."
    exit 3
  fi
  echo "PASS."
  exit 0
}

# Did we expect the current test, as set by start_test, to fail?
function is_expected_failure() {
  # Does PAGESPEED_EXPECTED_FAILURES contain CURRENT_TEST?
  test "$PAGESPEED_EXPECTED_FAILURES" != \
       "${PAGESPEED_EXPECTED_FAILURES/~"${CURRENT_TEST}"~/}"
}

# By default, print a message like:
#   failure at line 374
#   FAIL
# and then exit with return value 1.  If we expected this test to fail, log to
# $EXPECTED_FAILURES and return without exiting.
#
# If the shell does not support the 'caller' builtin, skip the line number info.
#
# Assumes it's being called from a failure-reporting function and that the
# actual failure the user is interested in is our caller's caller.  If it
# weren't for this, fail and handle_failure could be the same.
function handle_failure() {
  if [ $# -eq 1 ]; then
    echo FAILed Input: "$1"
  fi

  # From http://stackoverflow.com/questions/685435/bash-stacktrace
  # to avoid printing 'handle_failure' we start with 1 to skip get_stack caller
  local i
  local stack_size=${#FUNCNAME[@]}
  for (( i=1; i<$stack_size ; i++ )); do
    local func="${FUNCNAME[$i]}"
    [ -z "$func" ] && func=MAIN
    local line_number="${BASH_LINENO[(( i - 1 ))]}"
    local src="${BASH_SOURCE[$i]}"
    [ -z "$src" ] && src=non_file_source
    local canonical_dir=$(cd $(dirname "$src") && pwd)
    local short_dir=${canonical_dir#*/net/instaweb/}
    local leaf=$(basename "$src")
    echo "${short_dir}/${leaf}:${line_number}: $func"
  done

  # Note: we print line number after "failed input" so that it doesn't get
  # knocked out of the terminal buffer.
  if type caller > /dev/null 2>&1 ; then
    # "caller 1" is our caller's caller.
    echo "     failure at line $(caller 1 | sed 's/ .*//')" 1>&2
  fi
  echo "in '$CURRENT_TEST'"
  if is_expected_failure ; then
    echo "$CURRENT_TEST" >> "$EXPECTED_FAILURES"
    echo "Continuing after expected failure..."
  else
    echo FAIL.
    exit 1;
  fi
}

# Call with a command and its args.  Echos the command, then tries to eval it.
# If it returns false, fail the tests.
function check() {
  echo "     check" "$@"
  "$@" || handle_failure
}

# Like check, but the first argument is text to pipe into the command given in
# the remaining arguments.
function check_from() {
  local quiet=0
  if [ "$1" = "-q" ]; then
    quiet=1
    shift
  fi
  local text="$1"
  local msg="$text"
  shift
  if [ "$quiet" -ne 0 ];then
    msg="(check_from -q $@): $text"
  else
    echo "     check_from" "$@"
  fi
  echo "$text" | "$@" || handle_failure "$msg"
}

# Same as check(), but expects command to fail.
function check_not() {
  echo "     check_not" "$@"
  if "$@"; then
    handle_failure
  fi
}

# Runs a command and verifies that it exits with an expected error code.
function check_error_code() {
  local expected_error_code="$1"
  shift
  echo "     check_error_code $expected_error_code $@"
  local error_code=0
  "$@" || error_code="$?"
  check [ "$error_code" = "$expected_error_code" ]
}

# Like check_not, but the first argument is text to pipe into the
# command given in the remaining arguments.
function check_not_from() {
  local text="$1"
  shift
  echo "     check_not_from" "$@"
  if echo "$text" | "$@"; then
    handle_failure "$text"
  fi
}

function check_200_http_response() {
  check_from "$(head -1 <<< $1)" egrep -q '[ ]*HTTP/1[.]. 200 OK'
}

function check_200_http_response_file() {
  check_200_http_response "$(< $1)"
}

# Check for the existence of a single file matching the pattern
# in $1.  If it does not exist, print an error.  If it does exist,
# check that its size meets constraint identified with $2 $3, e.g.
#   check_file_size "$WGET_DIR/xPuzzle*" -le 60000
function check_file_size() {
  local filename_pattern="$1"
  local op="$2"
  local expected_value="$3"
  local SIZE=$(stat -c %s $filename_pattern) || handle_failure \
      "$filename_pattern not found"
  [ "$SIZE" "$op" "$expected_value" ] || handle_failure \
      "$filename_pattern : $SIZE $op $expected_value"
}

# In a pipeline a failed check or check_not will not halt the script on error.
# Instead of:
#   echo foo | check grep foo
# You need:
#   echo foo | grep foo || fail
# If you can legibly rewrite the code not to need a pipeline at all, however,
# check_from is better because it can print the problem test and the failing
# input on failure:
#   check_from "foo" grep foo
function fail() {
  handle_failure
}

function get_stat() {
  grep -w "$1" | awk '{print $2}' | tr -d ' '
}

function check_stat() {
  check_stat_op $1 $2 $3 $4 =
}

function check_stat_op() {
  if [ "${statistics_enabled:-1}" -eq "0" ]; then
    return
  fi
  local OLD_STATS_FILE=$1
  local NEW_STATS_FILE=$2
  local COUNTER_NAME=$3
  local EXPECTED_DIFF=$4
  local OP=$5
  local OLD_VAL=$(get_stat ${COUNTER_NAME} <${OLD_STATS_FILE})
  local NEW_VAL=$(get_stat ${COUNTER_NAME} <${NEW_STATS_FILE})

  # This extra check is necessary because the syntax error in the second if
  # does not cause bash to fail :/
  if [ "${NEW_VAL}" != "" -a "${OLD_VAL}" != "" ]; then
    if [ $((${NEW_VAL} - ${OLD_VAL})) $OP ${EXPECTED_DIFF} ]; then
      return;
    fi
  fi

  # Failure
  local EXPECTED_VAL=$((${OLD_VAL} + ${EXPECTED_DIFF}))
  echo -n "Mismatched counter value : ${COUNTER_NAME} : "
  echo "Expected(${EXPECTED_VAL}) $OP Actual(${NEW_VAL})"
  echo "Compare stat files ${OLD_STATS_FILE} and ${NEW_STATS_FILE}"
  handle_failure
}

# Continuously fetches URL and pipes the output to COMMAND.  Loops until COMMAND
# outputs RESULT, in which case we return 0, or until TIMEOUT seconds have
# passed, in which case we return 1.
#
# Usage:
#    fetch_until [-save] [-recursive] REQUESTURL COMMAND RESULT [WGET_ARGS] [OP]
#
# If "-save" is specified as the first argument, then the output from $COMMAND
# is retained in $FETCH_UNTIL_OUTFILE.
#
# If "-recursive" is specified, then the resources referenced from the HTML
# file are loaded into $WGET_DIR as a result of this command.
function fetch_until() {
  FETCH_UNTIL_OUTFILE="$WGET_DIR/fetch_until_output.$$"

  local save=0
  if [ "$1" = "-save" ]; then
    save=1
    shift
  fi

  local gzip=""
  if [ "$1" = "-gzip" ]; then
    gzip="--header=Accept-Encoding:gzip"
    shift
  fi

  local recursive=0
  if [ "$1" = "-recursive" ]; then
    recursive=1
    shift
  fi

  local expect_time_out=0
  if [ "$1" = "-expect_time_out" ]; then
    expect_time_out=1
    shift
  fi

  REQUESTURL=$1
  COMMAND=$2
  EXPECTED_RESULT=$3

  local wget_arg="${4:-}"
  if [[ "$wget_arg" == --user-agent=webp* ]]; then
    wget_arg="$wget_arg --header=Accept:image/webp"
    shift
  fi
  FETCH_UNTIL_WGET_ARGS="$gzip $WGET_ARGS $wget_arg"
  OP=${5:-=}  # Default to =

  if [ $recursive -eq 1 ]; then
    FETCH_FILE="$WGET_DIR/$(basename $REQUESTURL)"
    FETCH_UNTIL_WGET_ARGS="$FETCH_UNTIL_WGET_ARGS $PREREQ_ARGS"
  else
    FETCH_FILE="$FETCH_UNTIL_OUTFILE"
    FETCH_UNTIL_WGET_ARGS="$FETCH_UNTIL_WGET_ARGS -o $WGET_OUTPUT \
                                                  -O $FETCH_FILE"
  fi

  # TIMEOUT is how long to keep trying, in seconds.
  if is_expected_failure ; then
    # For tests that we expect to fail, don't wait long hoping for the right
    # result.
    TIMEOUT=10
  elif [ $expect_time_out -eq 1 ]; then
    # So far, all images tested in this mode are completed in 200 milliseconds
    # in non-valgrind mode. To make the test robust, we set the threshold to 5x,
    # and then another 5x for valgrind mode.
    if [ "${USE_VALGRIND:-}" = true ]; then
      TIMEOUT=5
    else
      TIMEOUT=1
    fi
  else
    # Foreground tests shouldn't wait as long as background tests can, but still
    # longer than you'd think we'd need, because of Valgrind.
    TIMEOUT=100
  fi

  START=$(date +%s)
  STOP=$((START+$TIMEOUT))
  WGET_HERE="$WGET -q $FETCH_UNTIL_WGET_ARGS"
  echo -n "      Fetching $REQUESTURL $FETCH_UNTIL_WGET_ARGS"
  echo " until \$($COMMAND) $OP $EXPECTED_RESULT"
  while test -t; do
    # Clean out WGET_DIR so that wget doesn't create .1 files.
    rm -rf $WGET_DIR
    mkdir -p $WGET_DIR

    $WGET_HERE $REQUESTURL || true
    ACTUAL_RESULT=$($COMMAND < "$FETCH_FILE" || true)
    if [ "$ACTUAL_RESULT" "$OP" "$EXPECTED_RESULT" ]; then
      echo "."
      if [ $save -eq 0 ]; then
        if [ $recursive -eq 1 ]; then
          rm -rf $WGET_DIR
        else
          rm -f "$FETCH_FILE"
        fi
      fi
      return;
    fi
    if [ $(date +%s) -gt $STOP ]; then
      echo ""
      if [ $expect_time_out -eq 1 ]; then
        echo "TIMEOUT: expected"
      else
        local file_size=$(cat "$FETCH_FILE" | wc -c)
        local file_mime=$(file -ib "$FETCH_FILE")

        if echo "$file_mime" | grep -q "^text/"; then
          # Dump the beginning of the file, if it's text.
          echo "Fetched file: $file_size bytes ("
          cat "$FETCH_FILE" | head -n 100
          echo ")"
        else
          # Otherwise dump the beginning of the file as hex.
          echo "Fetched file: $file_size bytes, $file_mime begins ("
          xxd -l 256 "$FETCH_FILE"
          echo ")"
        fi
        echo "TIMEOUT: $WGET_HERE $REQUESTURL output in $FETCH_FILE"
        handle_failure
      fi
      return
    fi
    echo -n "."
    sleep 0.1
  done;
}

# Helper to set up most filter tests.  Alternate between using:
#  1) query-params vs request-headers
#  2) ModPagespeed... vs PageSpeed...
# to enable the filter so we know all combinations work.
filter_spec_method="query_params"
function test_filter() {
  rm -rf $OUTDIR
  mkdir -p $OUTDIR
  FILTER_NAME=$1;
  shift;
  FILTER_DESCRIPTION=$@
  start_test $FILTER_NAME $FILTER_DESCRIPTION
  # Filename is the name of the first filter only.
  FILE=${FILTER_NAME%%,*}.html
  if [ $filter_spec_method = "query_params" ]; then
    WGET_ARGS=""
    FILE="$FILE?ModPagespeedFilters=$FILTER_NAME"
    filter_spec_method="query_params_pagespeed"
  elif [ $filter_spec_method = "query_params_pagespeed" ]; then
    WGET_ARGS=""
    FILE="$FILE?PageSpeedFilters=$FILTER_NAME"
    filter_spec_method="headers"
  elif [ $filter_spec_method = "headers" ]; then
    WGET_ARGS="--header=ModPagespeedFilters:$FILTER_NAME"
    filter_spec_method="headers_pagespeed"
  else
    WGET_ARGS="--header=ModPagespeedFilters:$FILTER_NAME"
    filter_spec_method="query_params"
  fi
  URL=$EXAMPLE_ROOT/$FILE
  FETCHED=$WGET_DIR/$FILE
}

# Helper to test if we mess up extensions on requests to broken url
function test_resource_ext_corruption() {
  URL=$1
  RESOURCE=$2

  # Make sure the resource is actually there, that the test isn't broken
  echo checking that wgetting $URL finds $RESOURCE ...
  OUT=$($WGET_DUMP $WGET_ARGS $URL)
  check_from "$OUT" fgrep -qi $RESOURCE

  # Now fetch the broken version. This should succeed anyway, as we now
  # ignore the noise.
  check $WGET_PREREQ $WGET_ARGS "${EXAMPLE_ROOT}/${RESOURCE}broken"

  # Fetch normal again; ensure rewritten url for RESOURCE doesn't contain broken
  OUT=$($WGET_DUMP $WGET_ARGS $URL)
  check_not_from "$OUT" fgrep "broken"
}

function scrape_pipe_stat {
  egrep "^$1:? " | awk '{print $2}'
}

# Scrapes the specified statistic, returning the statistic value.
function scrape_stat {
  $WGET_DUMP $STATISTICS_URL | scrape_pipe_stat "$1"
}

function scrape_header {
  # Extracts the value from wget's emitted headers.  We use " " as a delimeter
  # here to avoid a leading space on the returned string.  Note also that wget
  # always generates "name: value\r", never "name:value\r".
  tr -s '\r\n' '\n'| egrep -ia "^.?$1:" | rev | cut -d' ' -f 1 | rev
}

# Scrapes HTTP headers from stdin for Content-Length and returns the value.
function scrape_content_length {
  scrape_header 'Content-Length'
}

# Pulls the headers out of a 'wget --save-headers' dump.
function extract_headers {
  local carriage_return=$(printf "\r")
  local last_line_number=$(
    grep --text -n \^${carriage_return}\$ $1 | cut -f1 -d:)
  head --lines=$last_line_number "$1" | sed -e "s/$carriage_return//"
}

# Extracts the cookies from a 'wget --save-headers' dump.
function extract_cookies {
  grep "Set-Cookie" | \
  sed -e 's/;.*//' -e 's/^.*Set-Cookie: */ --header=Cookie:/'
}

# Returns the "URL" suitable for either Apache or Nginx
function generate_url {
  DOMAIN="$1"  # Must not have leading 'http://'
  PATH="$2"    # Must have leading '/'.
  if [ -z "${STATIC_DOMAIN:-}" ]; then
    RESULT="http://$DOMAIN$PATH"
  else
    RESULT="--header X-Google-Pagespeed-Config-Domain:$DOMAIN"
    RESULT+=" http://$STATIC_DOMAIN$PATH"
  fi
  echo $RESULT
}

# Performs timed reads on the output from a command passed via $1. The stream
# will be interpreted as a chunked http encoding. Each chunk will be allowed
# at most threshold_sec ($2) seconds to be read or the function will fail. When
# the stream is fully read, the funcion will compare the total number of http
# chunks read with expect_chunk_count ($3) and fail on mismatch.
# Usage: check_flushing 5 1
# This will check if the curl command resulted in single chunk which was read
# within one second or less.
function check_flushing() {
  local hostname="$1"
  local threshold_sec="$2"
  local expect_chunk_count="$3"
  local output=""
  local start=$(date +%s%N)
  local chunk_count=0

  local base_url="http://$hostname.example.com/mod_pagespeed_test"
  local command="$CURL -f -N --raw -sS --proxy $SECONDARY_HOSTNAME"

  if [ "${USE_VALGRIND:-}" = true ]; then
    # We can't say much about correctness of timings under valgrind, so relax
    # the test for that.
    threshold_sec=$(echo "scale=2; $threshold_sec*10" | bc)
  fi

  # First make sure php is working and we can actually fetch this page.
  check $command "$base_url/php_withoutflush.php" -o /dev/null

  while true; do
    start=$(date +%s%N)
    # Read the http chunk size from the stream. This is also the read which
    # checks timings.
    check read -t $threshold_sec line
    echo "Chunk number [$chunk_count] has size: $line"
    line=$(echo $line | tr -d '\n' | tr -d '\r')
    # If we read 0 that means we have finished reading the stream.
    if [ $((16#$line)) -eq "0" ] ; then
      check [ $expect_chunk_count -le $chunk_count ]
      return
    fi
    let chunk_count=chunk_count+1
    # read the actual data from the stream, using the amount indicated in
    # the previous read. This read should be fast.
    # Note that we need to clear IFS for read since otherwise it can get
    # confused by whitespace-only chunks.
    IFS= check read -N $((16#$line)) line
    echo "Chunk data: $line"
    # Read the trailing \r\n - should be fast.
    check read -N 2 line
  done < <($command "$base_url/slow_flushing_html_response.php")
  # Only reached if we finish the stream without a chunk of 0, which is an HTTP
  # protocol violation.
  fail
}

# Given the output of a page with ?PageSpeedFilters=+debug, print the section of
# the page where it lists what filters are enabled.
function extract_filters_from_debug_html() {
  local debug_output="$1"

  # Pull out the non-blank lines between "Filters:" and Options:".  First
  # convert newlines to % so sed can operate on the whole file, then put them
  # back again.
  check_from -q "$debug_output" grep -q "^Filters:$"
  check_from -q "$debug_output" grep -q "^Options:$"
  echo "$debug_output" | tr '\n' '%' | sed 's~.*%Filters:%~~' \
                       | sed "s~%Options:.*~~" | tr '%' '\n'
}

# The prioritize_critical_css test is split into two functions so
# nginx_system_test.sh can verify that beacon data is preserved across restarts
# via shm-cache checkpointing.  Specifically, the nginx system test first does a
# run of test_prioritize_critical_css, restarts nginx, and then runs
# test_prioritize_critical_css_final.  Because beacon responses are saved in the
# metadata cache this can only pass if the metadata cache is being persisted
# across restarts.
#
# That means this test is run twice when testing, both here and then again later
# on either side of a restart, but it's pretty fast so that's not a problem.
function test_prioritize_critical_css() {
  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    # Test critical CSS beacon injection, beacon return, and computation.  This
    # requires UseBeaconResultsInFilters() to be true in rewrite_driver_factory.
    # NOTE: must occur after cache flush, which is why it's in this embedded
    # block.  The flush removes pre-existing beacon results from the pcache.
    test_filter prioritize_critical_css
    fetch_until -save $URL 'fgrep -c pagespeed.criticalCssBeaconInit' 1
    check [ $(fgrep -o ".very_large_class_name_" $FETCH_FILE | wc -l) -eq 36 ]
    CALL_PAT=".*criticalCssBeaconInit("
    SKIP_ARG="[^,]*,"
    CAPTURE_ARG="'\([^']*\)'.*"
    BEACON_PATH=$(sed -n "s/${CALL_PAT}${CAPTURE_ARG}/\1/p" $FETCH_FILE)
    ESCAPED_URL=$(sed -n \
      "s/${CALL_PAT}${SKIP_ARG}${CAPTURE_ARG}/\1/p" $FETCH_FILE)
    OPTIONS_HASH=$(sed -n \
      "s/${CALL_PAT}${SKIP_ARG}${SKIP_ARG}${CAPTURE_ARG}/\1/p" $FETCH_FILE)
    NONCE=$(sed -n \
      "s/${CALL_PAT}${SKIP_ARG}${SKIP_ARG}${SKIP_ARG}${CAPTURE_ARG}/\1/p" \
      $FETCH_FILE)
    BEACON_URL="http://${HOSTNAME}${BEACON_PATH}?url=${ESCAPED_URL}"
    BEACON_DATA="oh=${OPTIONS_HASH}&n=${NONCE}&cs=.big,.blue,.bold,.foo"

    OUT=$($CURL -sSi -d "$BEACON_DATA" "$BEACON_URL")
    check_from "$OUT" grep '^HTTP/1.1 204'

    test_prioritize_critical_css_final
  fi
}

function test_prioritize_critical_css_final() {
  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    # Now make sure we see the correct critical css rules.
    fetch_until $URL \
      'grep -c <style>[.]blue{[^}]*}</style>' 1
    fetch_until $URL \
      'grep -c <style>[.]big{[^}]*}</style>' 1
    fetch_until $URL \
      'grep -c <style>[.]blue{[^}]*}[.]bold{[^}]*}</style>' 1
    fetch_until -save $URL \
      'grep -c <style>[.]foo{[^}]*}</style>' 1
    # The last one should also have the other 3, too.
    check [ `grep -c '<style>[.]blue{[^}]*}</style>' $FETCH_UNTIL_OUTFILE` = 1 ]
    check [ `grep -c '<style>[.]big{[^}]*}</style>' $FETCH_UNTIL_OUTFILE` = 1 ]
    check [ `grep -c '<style>[.]blue{[^}]*}[.]bold{[^}]*}</style>' \
      $FETCH_UNTIL_OUTFILE` = 1 ]
  fi
}

function cache_purge_test() {
  # Tests for individual URL purging, and for global cache purging via
  # GET pagespeed_admin/cache?purge=URL, and PURGE URL methods.
  PURGE_ROOT="$1"
  PURGE_STATS_URL="$PURGE_ROOT/pagespeed_admin/statistics"
  function cache_purge() {
    local purge_method="$1"
    local purge_path="$2"
    if [ "$purge_method" = "GET" ]; then
      echo http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          "$PURGE_ROOT/pagespeed_admin/cache?purge=$purge_path"
      http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          "$PURGE_ROOT/pagespeed_admin/cache?purge=$purge_path"
    else
      PURGE_URL="$PURGE_ROOT/$purge_path"
      echo $CURL --request PURGE --proxy $SECONDARY_HOSTNAME "$PURGE_URL"
      check $CURL --request PURGE --proxy $SECONDARY_HOSTNAME "$PURGE_URL"
    fi
    echo ""
    if [ $statistics_enabled -eq "0" ]; then
      # Without statistics, we have no mechanism to transmit state-changes
      # from one Apache child process to another, and so each process must
      # independently poll the cache.purge file, which happens every 5 seconds.
      echo sleep 6
      sleep 6
    fi
  }

  # Checks to see whether a .pagespeed URL is present in the metadata cache.
  # A response including "cache_ok:true" or "cache_ok:false" is send to stdout.
  function read_metadata_cache() {
    path="$PURGE_ROOT/$1"
    http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          "$PURGE_ROOT/pagespeed_admin/cache?url=$path"
  }

  # Find the full .pagespeed. URL of yellow.css
  PURGE_COMBINE_CSS="$PURGE_ROOT/combine_css.html"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
  yellow_css=$(grep yellow.css $FETCH_UNTIL_OUTFILE | cut -d\" -f6)
  blue_css=$(grep blue.css $FETCH_UNTIL_OUTFILE | cut -d\" -f6)

  purple_path="styles/$$"
  purple_url="$PURGE_ROOT/$purple_path/purple.css"
  purple_dir="$APACHE_DOC_ROOT/purge/$purple_path"
  ls -ld $APACHE_DOC_ROOT $APACHE_DOC_ROOT/purge
  echo $SUDO mkdir -p "$purple_dir"
  $SUDO mkdir -p "$purple_dir"
  purple_file="$purple_dir/purple.css"

  for method in $CACHE_PURGE_METHODS; do
    echo Individual URL Cache Purging with $method
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:true
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:true
    echo 'body { background: MediumPurple; }' > "/tmp/purple.$$"
    $SUDO cp "/tmp/purple.$$" "$purple_file"
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$purple_url" 'fgrep -c 9370db' 1
    echo 'body { background: black; }' > "/tmp/purple.$$"
    $SUDO cp "/tmp/purple.$$" "$purple_file"

    cache_purge $method "*"

    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:false
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:false
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$purple_url" 'fgrep -c #000' 1
    cache_purge "$method" "$purple_path/purple.css"

    sleep 1
    STATS=$OUTDIR/purge.stats
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.0
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.1

    # Having rewritten 4 CSS files, we will have done 4 resources fetches.
    check_stat $STATS.0 $STATS.1 num_resource_fetch_successes 4

    # Sanity check: rewriting the same CSS file results in no new fetches.
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.2
    check_stat $STATS.1 $STATS.2 num_resource_fetch_successes 0

    # Now flush one of the files, and it should be the only one that
    # needs to be refetched after we get the combine_css file again.
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:true
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:true
    cache_purge $method styles/yellow.css
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:false
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:true

    sleep 1
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.3
    check_stat $STATS.2 $STATS.3 num_resource_fetch_successes 1
  done
  $SUDO rm -rf "$purple_dir" "/tmp/purple.$$"
}
