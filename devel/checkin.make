#!/bin/bash
#
# This script is intended to be run from 'checkin'.  It runs a series of tests,
# noting which ones failed, and allowing re-running only the failed ones if
# needed.

this_dir=$(dirname "${BASH_SOURCE[0]}")
cd "$this_dir"

# When a single system test fails, keep running until the end of the test
# script, and then print out all failing tests.  While this isn't a better flow
# for interactive use for all users, for system tests it allows you to see the
# full list of system tests that failed so you can iterate on them or test them
# for flakiness.
export CONTINUE_AFTER_FAILURE=true

source checkin_test_helpers.sh

export OBJDIR=${OBJDIR:-/tmp/instaweb.$$}
make_args_array=($MAKE_ARGS)
mkdir -p "$OBJDIR"

failed_tests=""
prep_failures=""
for prep in $(make echo_checkin_prep); do
  run_noisy_command_showing_log "$OBJDIR"/"$prep".log "$prep" \
    make "${make_args_array[@]}" "$prep"
  if [ "$?" -ne "0" ]; then
    prep_failures+=" $prep"
  fi
done

if [ ! -z "$prep_failures" ]; then
  echo checkin_prep failed: "$prep_failures"
  exit 1
fi

if [ "$#" -eq 0 ]; then
  tests=( \
    apache_test \
    apache_release_test \
    apache_system_tests \
    pagespeed_automatic_smoke_test \
  )
else
  tests=("$@")
fi

for testname in "${tests[@]}"; do
  is_system_test=$(echo "$testname" | grep -c system_test)
  if [ "$is_system_test" = 1 ]; then
    SERVER="Apache"
    LOCKFILE="$APACHE_LOCKFILE"
    echo -n Waiting for "$SERVER" lock "$LOCKFILE" ...
    acquire_lock "$SERVER" "$LOCKFILE"
    print_elapsed_time
    echo ""
  fi
  run_noisy_command_showing_log "$OBJDIR/${testname}.log" "$testname" \
    make "${make_args_array[@]}" "$testname"
  if [ "$?" -ne "0" ]; then
    failed_tests+=" $testname"
  fi
  if [ "$is_system_test" = 1 ]; then
    run_noisy_command_showing_log "$OBJDIR/apache_install_conf.log" \
      "Returning Apache config to a consistent state." \
      make "${make_args_array[@]}" apache_install_conf
    release_lock "$SERVER" "$LOCKFILE"
  fi
done

if [ -z "$failed_tests" ]; then
  echo "All 'make' tests passed."
  exit 0
fi

echo Failing tests: "$failed_tests"
echo Re-run with devel/checkin.make "$failed_tests"
exit 1
