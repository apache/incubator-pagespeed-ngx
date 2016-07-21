#!/bin/bash
#
# Helping script which runs arbitrary server process (e.g. memcached or
# redis-server) in a temporary directory on random available port.
#
# All you have to do is to run `start_server` function and pass command which
# runs server (arguments possible). It will be directly evaluated 'as-is' with
# eval builtin. An EXIT trap will be automatically set up which kills server
# and removes temporary working directory. If there was an EXIT trap installed
# already (e.g. this script has been ran before), script fails immediately.
#
# WARNING: server should not daemonize itself, otherwise script won't be able
# to grasp pid and kill the server.
#
# After the script ends, you have server running in background and can access
# SERVER_PID, SERVER_PORT and SERVER_WORKDIR variables if further configuration
# is needed.
#
# This scripts assumes that first argument is a valid command and that quoted
# $SERVER_PORT occurs somewhere in arguments. This is not necessary for it to
# work, but allows us to do extra sanity checks.
#
# TODO(yeputons): make starting several servers with consecutive runs of script
# possible.

set -e
set -u

# Check amount of arguments
if [[ "$#" == 0 ]]; then
  echo "Usage: $0 <server-start-command> [<arguments...>]" >&2
  exit 1
fi
SERVER_CMD="$1"

# Probe command
if ! which "$SERVER_CMD" >/dev/null; then
  echo "Unable to locate $SERVER_CMD." >&2
  echo "Try running 'sudo apt-get install $SERVER_CMD'." >&2
  exit 1
fi

# Sanity check: if command does not use $SERVER_PORT, why would server listen on
# the port we chose?
if ! ( echo "$@" | grep --quiet '$SERVER_PORT'  ); then
  echo 'Looks like you do not use $SERVER_PORT in server command.'   >&2
  echo 'If you do, make sure to use single quotes around so it is'   >&2
  echo 'substituted inside the script, not when you call the script' >&2
  exit 1
fi

# Check that there is no existing EXIT trap, otherwise we would override it
if [[ -n "$(trap -p EXIT)" ]]; then
  echo "EXIT trap is already set up, failing" >&2
  exit 1
fi

SERVER_PID=""
SERVER_PORT=""
SERVER_WORKDIR=$(mktemp -d)

# Function called on bash's termination to clean up
function kill_server {
  if [[ -n "$SERVER_PID" ]]; then
    echo "Killing $SERVER_CMD on port $SERVER_PORT running with pid=$SERVER_PID"
    kill "$SERVER_PID" || true
  fi
  rm -rf "$SERVER_WORKDIR" || true
}
trap 'kill_server' EXIT

# Pick random ports until we successfully can run server.
while [[ -z "$SERVER_PORT" ]]; do
  # Pick a port between 1024 and 32767 inclusive.
  SERVER_PORT=$((($RANDOM % 31744) + 1024))

  # First check netstat -tan to see if somone is already listening on this port.
  if nenstat -tan 2>/dev/null | grep --quiet ":$SERVER_PORT .* LISTEN"; then
    continue
  fi

  echo "Trying to start $SERVER_CMD on port $SERVER_PORT"

  # & is quoted because we want run command in background, not whole eval
  eval "$@" "&"

  SERVER_PID="$!"
  sleep 2 # wait until server starts

  # Check that the process we started actually listens for the port we want
  if ! ( netstat -tanp 2>/dev/null |
    grep ":$SERVER_PORT .* LISTEN .* $SERVER_PID/$SERVER_CMD" >/dev/null); then
    echo "$SERVER_CMD does not listen on port $SERVER_PORT after two seconds, killing it"
    kill "$SERVER_PID" || true
    SERVER_PID=""
    SERVER_PORT=""
  fi
done

echo $SERVER_CMD is up and running on port $SERVER_PORT with pid=$SERVER_PID
