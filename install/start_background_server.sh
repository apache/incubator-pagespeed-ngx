#!/bin/bash
#
# Copyright 2016 Google Inc.
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
# Helping script which runs arbitrary server process (e.g. memcached or
# redis-server) in a temporary directory on random available port.
#
# All you have to do is to `source` this script and pass command which runs
# server (arguments possible). It will be directly evaluated 'as-is' with eval
# builtin. An EXIT trap will be automatically set up which kills server and
# removes temporary working directory. If there was an EXIT trap installed
# already (e.g. this script has been run before), script fails immediately. As a
# workaround, you can start that script in a subshell.
#
# WARNING: server should not daemonize itself, otherwise script won't be able
# to grasp pid and kill the server.
#
# WARNING: one should not use `exec` after calling this script because `exec`
# will effectively remove bash's EXIT trap which terminates the server.
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

source $(dirname "$BASH_SOURCE")/shell_utils.sh || exit 1

# Check amount of arguments
if [[ "$#" == 0 ]]; then
  echo "Usage: $0 <server-start-command> [<arguments...>]" >&2
  exit 1
fi

# Probe command
if ! which "$1" >/dev/null; then
  echo "Unable to locate $1." >&2
  echo "Try running 'sudo apt-get install $1'." >&2
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

SERVER_CMD="$1"
SERVER_NAME=$(basename "$1")
SERVER_PID=""
SERVER_PORT=""
SERVER_WORKDIR=$(mktemp -d)

# Function called on bash's termination to clean up
function kill_server {
  if [[ -n "$SERVER_PID" ]]; then
    echo "Killing $SERVER_NAME on port $SERVER_PORT running with pid=$SERVER_PID"
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

  echo "Trying to start $SERVER_NAME on port $SERVER_PORT"

  # & is quoted because we want run command in background, not whole eval
  eval "$@" "&"

  SERVER_PID="$!"

  echo -n "Waiting for server..."
  if ! wait_cmd_with_timeout 2 \
    'netstat -tanp 2>/dev/null |' \
    'grep ":$SERVER_PORT .* LISTEN .* $SERVER_PID/$SERVER_NAME" >/dev/null'
  then
    echo
    echo "$SERVER_NAME does not listen on port $SERVER_PORT after two" \
         "seconds, killing it"
    kill "$SERVER_PID" || true
    SERVER_PID=""
    SERVER_PORT=""
  else
    echo
  fi
done

echo $SERVER_NAME is up and running on port $SERVER_PORT with pid=$SERVER_PID
