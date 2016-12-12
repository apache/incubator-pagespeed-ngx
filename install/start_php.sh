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

# Starts a php-cgi server on the specified port.  If there's already one running
# on that port that it looks like it started, kills that one first.
#
# Usage: start_php.sh <port>
# Example: start_php.sh 9000

DISABLE_PHP_TESTS="${DISABLE_PHP_TESTS:-}"
if [ -n "$DISABLE_PHP_TESTS" ]; then
  echo "PHP tests are disabled; skipping PHP restart."
  exit 0
fi

port="$1"

if ! command -v php-cgi &> /dev/null; then
  echo "php-cgi doesn't seem to be available, please install it."
  echo ""
  echo "On debian-derived systems this is:"
  echo "  sudo apt-get install php5-cgi"
  echo ""
  echo "Alternatively, you can export DISABLE_PHP_TESTS=1 to skip tests that"
  echo "depend on php."
  exit 1
fi

args="-b 127.0.0.1:$port"

function get_php_pid() {
  ps auxw | awk '/php-cg[i] '"$args"'/{print $2}'
}

existing_php_pid="$(get_php_pid)"
if [ "$existing_php_pid" != "" ]; then
  echo -n "PHP already running on $port with pid $existing_php_pid, killing it."
  if ! kill "$existing_php_pid"; then
    echo
    echo "Failed to kill php."
    exit 1
  fi
  # Wait for php to actually die.
  timeout="$(($SECONDS + 10))"
  while [ "$SECONDS" -lt "$timeout" ] && [ "$(get_php_pid)" != "" ]; do
    sleep 0.1
    echo -n .
  done
  echo
  if [ "$(get_php_pid)" != "" ]; then
    echo "php did not die."
    exit 1
  fi
fi

echo -n "Starting PHP on port $port..."
REDIRECT_STATUS=1 php-cgi $args &
new_php_pid="$!"
timeout="$(($SECONDS + 10))"
while [ "$SECONDS" -lt "$timeout" ] && \
      [ "$(get_php_pid)" != "$new_php_pid" ]; do
  sleep 0.1
  echo -n .
done

echo
if [ "$(get_php_pid)" != "$new_php_pid" ]; then
  echo "Failed to start php."
  exit 1
fi
echo "PHP running with PID $new_php_pid"

