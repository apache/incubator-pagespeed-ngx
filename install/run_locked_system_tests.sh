#!/bin/bash
#
# Copyright 2011 Google Inc. All Rights Reserved.
# Author: sligocki@google.com (Shawn Ligocki)
#
# Runs all system tests holding a lock so that checkin tests from two clients
# can be run simultaneously.
#
# Called from net/instaweb/Makefile:all

set -u

readonly LOCKDIR=/usr/local/apache2
readonly APACHE_LOCKFILE="$LOCKDIR/pagespeed.lock"
readonly NGINX_LOCKFILE="$LOCKDIR/ngx_pagespeed.lock"

if [ -z "$MAKE" ]; then
  echo "Must define evnironmental variable MAKE."
  exit 1
fi

function acquire_lock {
  SERVER=$1
  LOCKFILE=$2

  echo "Locking $SERVER."
  LOCK_PID=$(cat $LOCKFILE 2> /dev/null)
  if [ -e $LOCKFILE -a "$LOCK_PID" != "$$" ]; then
    echo "Someone else (PID=$LOCK_PID) is holding the $SERVER lock."

    echo "Waiting for lock"
    while [ -e $LOCKFILE ]; do
      echo -n "."
      if ! (ps -p $LOCK_PID | grep -q $LOCK_PID); then
        echo "Removing stale lock. Process PID=$LOCK_PID, no longer exists."
        rm $LOCKFILE
      else
        sleep 1
      fi
    done
  fi

  echo "Taking $SERVER lock (PID=$$)"
  echo "$$" > $LOCKFILE
  # Rudimentary race protection.
  if [ "$(cat $LOCKFILE)" != "$$" ]; then
    echo "Lost race to take lock. If this happens often refactor"
    echo "run_locked_system_tests.sh to deal with losing races better."
    exit 1
  fi
}

function release_lock {
  SERVER=$1
  LOCKFILE=$2

  echo "Unlocking $SERVER."
  rm $LOCKFILE
}

function run_apache_system_test {
  acquire_lock "Apache" $APACHE_LOCKFILE

  echo "Running all Apache system tests."
  $MAKE apache_system_tests
  local apache_status=$?

  echo "Returning Apache config to a consistent state."
  $MAKE apache_install_conf

  release_lock "Apache" $APACHE_LOCKFILE
  return $apache_status
}

function run_nginx_system_test {
  acquire_lock "Nginx" $NGINX_LOCKFILE

  echo "Running all Nginx system tests."
  $MAKE nginx_checkin_test
  local nginx_status=$?

  release_lock "Nginx" $NGINX_LOCKFILE
  return $nginx_status
}

# TODO(jefftk): deflake and set background testing to default on.
if [ "${RUN_TESTS_ASYNC:-off}" = "off" ]; then
  if ! run_nginx_system_test; then
    echo "Nginx test failed."
    exit 1
  fi
  if ! run_apache_system_test; then
    echo "Apache test failed."
    exit 1
  fi
  exit 0
fi

# We need pipefail so when we run the system tests in the background and tee
# their output we'll be able to see if they failed.
set -o pipefail

apache_log_file=/tmp/checkin.make.apache.$$
(run_apache_system_test |& tee $apache_log_file) &
apache_pid=$!

nginx_log_file=/tmp/checkin.make.nginx.$$
(run_nginx_system_test |& tee $nginx_log_file) &
nginx_pid=$!

# We can't just wait for all child processes with a single "wait" because we'd
# lose their exit codes.  So wait for one and then the other.  You might think
# there would be problems here if one finishes quickly enough that it's already
# complete by the time bash gets to calling wait, but bash records the exit
# statuses of processes [1] as they terminate so you can wait on a child process
# that has already ended and still get its exit status.
#
# [1] As posix requires:
#     http://pubs.opengroup.org/onlinepubs/9699919799/utilities/wait.html
overall_status=0
if wait $apache_pid; then
  apache_status="PASS"
  rm $apache_log_file
else
  apache_status="FAIL"
  overall_status=1
fi
if wait $nginx_pid; then
  nginx_status="PASS"
  rm $nginx_log_file
else
  nginx_status="FAIL"
  overall_status=1
fi

echo
echo "(On failure, details are in $apache_log_file and $nginx_log_file)"
echo
echo "APACHE_STATUS: $apache_status"
echo "NGINX_STATUS: $nginx_status"

exit $overall_status
