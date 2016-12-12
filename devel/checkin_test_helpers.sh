#!/bin/bash
#
# Copyright 2011 Google Inc.
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
# Author: sligocki@google.com (Shawn Ligocki)
#
# Helper functions for holding locks so that checkin tests from two clients
# can be run simultaneously, and printing status on long-running commands.
#
# Sourced from checkin.make.

readonly APACHE_LOCKFILE="/tmp/pagespeed-apache.lock"

function acquire_lock {
  local server=$1
  local lockfile=$2

  local lockfile_tmp="$lockfile.$$"
  local printed_msg=0

  echo $$ > "$lockfile_tmp"
  # ln will fail if $lockfile exists, making this an atomic test and set.
  # Note that this is a hard link (ln), not a symlink (ln -s).
  while ! ln "$lockfile_tmp" "$lockfile" 2>/dev/null; do
    local lock_pid=$(cat "$lockfile" 2>/dev/null)
    if [ "$lock_pid" = "$$" ]; then
      ## We already have the lock, apparently!
      break
    fi

    if [ -n "$lock_pid" ] && ! ps "$lock_pid" >/dev/null; then
      echo "Removing stale lock. Process PID=$lock_pid, no longer exists."
      rm "$lockfile"
    else
      if [ "$printed_msg" = 0 ]; then
        echo -n "Waiting for PID $lock_pid to give up the $server lock."
        printed_msg=1
      else
        echo -n '.'
      fi
      sleep 1
    fi
  done

  if [ "$printed_msg" != 0 ]; then
    echo
  fi
  rm -f "$lockfile_tmp"
}

function release_lock {
  SERVER=$1
  LOCKFILE=$2

  echo "Unlocking $SERVER."
  rm "$LOCKFILE"
}
exit_status=0

# Returns the unix system time in seconds.
function now_sec() {
  date +%s
}

start_time_sec=$(now_sec)

# Prints the elapsed time, in seconds, since the last time print_elapsed_time
# was called.  Any arguments to this function will be passed to through as the
# first args to echo.  The intent is you can put
#   print_elapsed_time -n
# to allow callers to print more stuff on the same line.
function print_elapsed_time() {
  current_time_sec=$(now_sec)
  if [ "$previous_time_sec" != 0 ]; then
    echo -n : "$((current_time_sec - start_time_sec))" sec
  fi
}

# Determines whether the passed-in PID is alive.
function is_process_alive() {
  ps "$1" > /dev/null
}

# Runs command, redirecting stdout+stderr to a logfile, which is specified as
# the first argument.  The second argument is a string to put in the status
# messsage.  This might be all or part of the actual command, or something
# descriptive.  The rest of the arguments are the command.
#
# The full command will be added as the first line of the logfile.
#
# This function blocks until the command finishes, but it prints out status
# lines at increasing intervals, with the max interval being 60 seconds.  Once
# the 60-second threshold is reached, each status line is emitted with a
# newline.  This is so that two long-running commands running in parallel
# don't completely overwrite each other's status.
#
# The global variable 'exit_status' is set to 0 if the command succeeds, 1 if
# it fails.
function run_noisy_command_showing_log() {
  logfile="$1"
  shift
  description="$1"
  shift

  start_time_sec=$(now_sec)
  previous_time_sec=$start_time_sec
  echo "$@" "&>" "$logfile" "..."
  ("$@" ; echo exit_status=$?) &> "$logfile" &
  pid=$!
  print_interval_sec=60
  while is_process_alive $pid; do
    sleep 1
    current_sec=$(now_sec)
    interval_sec=$((current_sec - previous_time_sec))
    if [ $interval_sec -ge $print_interval_sec ]; then
      previous_time_sec=$current_sec
      lines_in_logfile=$(wc -l < "$logfile")
      echo " ... $description: $lines_in_logfile lines$(print_elapsed_time)"
    fi
  done
  if [ "$(tail -n 1 "$logfile")" = "exit_status=0" ]; then
    echo -n "PASS"
    local exit_status=0
  else
    echo -n "FAIL"
    local exit_status=1
  fi
  print_elapsed_time
  # shellcheck disable=SC2145
  echo " ($@ >& $logfile)"
  return $exit_status
}
