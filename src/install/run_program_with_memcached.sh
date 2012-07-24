#!/bin/sh
#
# Checks that that memcached is already installed, and then runs it on
# port 6765.  The command in $@ is then run (e.g. a test binary)
# and memcached is then shut down.
#
# Due to limitations in the author's bash skills, this script is run in
# one of two modes:
#    .../run_program_with_memcached.sh -multi command one \; command two \; ...
# or
#    .../run_program_with_memcached.sh any one command allowing "quoted args"
#
# The latter is used for running unit tests, potentially with a quoted argument
# like  --gtest_filter="AprMemCache*".  The former is used for running system
# tests twice: one with cold-cache and once with warm-cache.

which memcached >/dev/null
if [ $? = 1 ]; then
  echo "***" Please run '"sudo apt-get install memcached"'
  exit 1
fi

memcached_pid="0"

ps auxww | grep -v grep | grep 'memcached -p 6765'
grep_status="$?"

set -u

if [ $grep_status = 0 ]; then
  echo memcached is running.
else
  echo starting memcached on port 6765, then sleeping for 2 seconds...
  memcached -p 6765 >/tmp/memcached.log &
  memcached_pid="$!"
  sleep 2
fi

exit_status="0"

# TODO(jmarantz): replace what's below with 'eval "$@"' or the like, eliminating
# the loop and special cases.
if [ "$1" = "-multi" ]; then
  shift
  cmd=""
  for arg in $@; do
    if [ "$arg" = ";" ]; then
      sh -c "$cmd"
      if [ $? != 0 ]; then
        exit_status="1"
      fi
      cmd=""
    else
      cmd+="$arg "
    fi
  done
  $cmd
  if [ $? != 0 ]; then
    exit_status="1"
  fi
else
  "$@"
  exit_status="$?"
fi

if [ "$memcached_pid" != "0" ]; then
  echo Killing memcached -p 6765 running in pid $memcached_pid
  kill $memcached_pid
fi

echo Exiting $0 with status $exit_status
exit $exit_status
