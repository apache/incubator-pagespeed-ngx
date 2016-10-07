#!/bin/bash

# Starts a php-cgi server on the specified port.  If there's already one running
# on that port that it looks like it started, kills that one first.
#
# Usage: start_php.sh <port>
# Example: start_php.sh 9000

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
  echo "PHP already running on $port with pid $existing_php_pid, killing it."
  if ! kill "$existing_php_pid"; then
    echo "Failed to kill php."
    exit 1
  fi
fi

echo "Starting PHP on port $port..."
php-cgi $args > /dev/null &
new_php_pid="$!"
if [ "$(get_php_pid)" != "$new_php_pid" ]; then
  echo "Failed to start php."
  exit 1
fi
echo "PHP running with PID $new_php_pid"

