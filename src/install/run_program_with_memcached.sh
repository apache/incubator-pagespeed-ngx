#!/bin/bash
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

set -u

which memcached >/dev/null
if [ $? = 1 ]; then
  echo "***" Please run '"sudo apt-get install memcached"'
  exit 1
fi

# If memcached is run as root, it expects an explicit -u user argument, or
# it will refuse to start. Normally one would want to use a restricted
# user, but for integration tests, root will do.
MEMCACHED_USER_OPTS=
if [ "$UID" = "0" ]; then
  MEMCACHED_USER_OPTS="-u root"
fi


# Pick random ports until we successfully can run memcached.
memcached_pid="0"
while [ $memcached_pid -eq "0" ]; do
  # Pick a port between 1024 and 32767 inclusive.
  port=$((($RANDOM % 31744) + 1024))

  # First check netstat -anp to see if somone is already listening on this port.
  if [ $(netstat -anp 2>&1 | grep -c "::$port .* LISTEN ") -eq 0 ]; then
    echo Trying memcached port $port
    memcached -p $port -m 1024 $MEMCACHED_USER_OPTS >/tmp/memcached.log &
    memcached_pid="$!"
    sleep 2

    # See if we are now listening on the port, and the process is still alive.
    if [ $(netstat -anp 2>&1 | grep -c "::$port .* LISTEN ") -gt 0 -a \
         $(ps $memcached_pid | grep -c memcached) -eq 1 ]; then
      # Provide an environment variable for use in apr_mem_cache_test.cc
      # and system tests, indicating what port # we used.
      export MEMCACHED_PORT=$port
    else
      echo -n memcached on port $port failed...
      cat /tmp/memcached.log
      kill $memcached_pid >& /dev/null
      memcached_pid="0"
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

if [ "$memcached_pid" != "0" ]; then
  echo Killing memcached -p $port running in pid $memcached_pid
  kill $memcached_pid
fi

echo Exiting $0 with status $exit_status
exit $exit_status
