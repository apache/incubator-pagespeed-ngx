#!/bin/bash
#
# Checks that that redis is already installed, and then runs it on
# random port with temporary working directory. The command in $@ is then run
# (e.g. a test binary) and redis is then shut down and temporary directory is
# deleted.
# This script was copied from run_program_with_memcache.sh and slightly modified
# TODO(yeputons): refactor these two scripts to decrease duplication and file it
#                 as a separate CL
#
# Due to limitations in the authors' bash skills, this script is run in
# one of two modes:
#    .../run_program_with_redis.sh -multi command one \; command two \; ...
# or
#    .../run_program_with_redis.sh any one command allowing "quoted args"
#
# The latter is used for running unit tests, potentially with a quoted argument
# like  --gtest_filter="RedisCache*".  The former is used for running system
# tests twice: one with cold-cache and once with warm-cache.

set -u

which redis-server >/dev/null
if [ $? = 1 ]; then
  echo "***" Please run '"sudo apt-get install redis-server"'
  exit 1
fi

redis_pid="0"
redis_workdir=$(mktemp -d)

function kill_redis {
  if [ "$redis_pid" != "0" ]; then
    echo Killing redis -p $port running in pid $redis_pid
    kill $redis_pid
  fi
  rm -rf $redis_workdir
}

trap 'kill_redis' EXIT

# Pick random ports until we successfully can run redis-server.
while [ $redis_pid -eq "0" ]; do
  # Pick a port between 1024 and 32767 inclusive.
  port=$((($RANDOM % 31744) + 1024))

  # First check netstat -anp to see if somone is already listening on this port.
  if [ $(netstat -tan | grep -c ":$port .* LISTEN ") -eq 0 ]; then
    echo Trying redis port $port
    redis-server --bind localhost --port $port --dir $redis_workdir --logfile /tmp/redis.log --maxmemory 1000000000 &
    redis_pid="$!"
    sleep 2

    # See if we are now listening on the port.
    if [ $(netstat -tanp 2>/dev/null | grep ":$port .* LISTEN " |\
             grep -c " $redis_pid/redis-server") -gt 0 ]; then
      # Provide an environment variable for use in redis_cache_test.cc
      # and system tests, indicating what port # we used.
      export REDIS_PORT=$port
    else
      echo -n redis on port $port failed...
      cat /tmp/redis.log
      kill $redis_pid >& /dev/null
      redis_pid="0"
    fi
  fi
done

exit_status="0"

set -e

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

echo Exiting $0 with status $exit_status
exit $exit_status
