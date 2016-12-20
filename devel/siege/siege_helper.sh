# This file expects to be sourced from another file in the same directory in
# order to set us up for siege testing.

set -u  # exit the script if any variable is uninitialized
set -e

if [ -d devel ]; then
  cd devel
fi
if [ ! -d siege ]; then
  echo Run this script from the top or devel/ directories
  exit 1
fi

if ! hash siege 2>/dev/null; then
  echo "'siege' command is not found, please install it. "
  echo "siege_instant_ipro needs 3.0.8 or newer, other tests work with 3.0.5 "
  echo "as well."
  exit 1
fi

# If an 'su' password is required, then get it before going off and compiling
# stuff.
./turn_on_timewait_recyling.sh

# Build optimized mod_pagespeed.so if necessary, and restart it.
callgrind=0
if [ $# -eq 1 ]; then
  if [ $1 == "-callgrind" ]; then
    shift
    callgrind=1
  fi
fi

# Stop callgrind if it was running previously.
callgrind_control -k

# Clear all caches to make sure we start from a known state.
make clean_slate_for_tests

# This variable contains config that we want to inject into siege.conf when
# constructing pagespeed.conf, by sed-replacing "CUSTOM_CONFIG".  This works
# with multiple config lines.
custom_config=""
function add_config_line() {
  custom_config+="\n    $1"
}

# If $MEMCACHED_PORT is set (i.e. we were run from
# run_program_with_memcached.sh) then configure it in the apache conf.
set +u
if [ ! -z "$MEMCACHED_PORT" ]; then
  add_config_line "ModPagespeedMemcachedServers localhost:$MEMCACHED_PORT"
fi
set -u

make apache_debug_stop
sed -e "s/#CUSTOM_CONFIG/$custom_config/" -e "s^#HOME^$HOME^" < siege/siege.conf \
  > ~/apache2/conf/pagespeed.conf

if [ $callgrind -eq 1 ]; then
  echo running with callgrind...
  make -j8 apache_debug_install CONF=OptDebug
  valgrind --tool=callgrind --collect-systime=yes ~/apache2/bin/httpd -X &
  sleep 5
  callgrind=1
else
  echo running without calgrind -- use -callgrind to get a profile.
  make -j8 apache_debug_restart BUILDTYPE=Release
fi

# This function returns its value in shell variable 'url'.  Note that it
# will return whatever is in the HTML value, which is usually a relative
# url.
function extract_pagespeed_url() {
  url=""
  html=$1
  grep_pattern="$2"
  url_token_index="$3"
  filters="$4"
  OPTIONS="?PageSpeedFilters=$filters"

  echo -n Finding pagespeed url in $html${OPTIONS}, pattern=\"${grep_pattern}\"
  echo ' #' $url_token_index
  while true; do
    LINE=$(wget -q -O - $html$OPTIONS | grep "$grep_pattern")
    if [ "$LINE" != '' ]; then
      url=$(echo $LINE | cut -d\" -f$url_token_index)
      echo $url
      break
    else
      sleep .1
      echo -n '.'
    fi
  done
}

this_file=$(basename "$0")
this_name=$(basename "$this_file" .sh)
common_options=("--log=/tmp/$this_name.log" --rc=/dev/null)

# Runs siege, passing on any provided arguments.
function run_siege_with_options() {
  (set -x; siege "${common_options[@]}" "$@")

  if [ $callgrind -eq 1 ]; then
    sleep 2
    callgrind_control -d
    ls -ltR callgrind.*
    echo Type \'callgrind_control -k\' to close down valgrind.
  fi

  (set -x; ./expectfail egrep "exit signal|CRASH" \
    ~/apache2/logs/error_log)
}

# Run siege on a set of arguments, with reasonable defaults.
function run_siege() {
  run_siege_with_options --benchmark --time=60s --concurrent=50 "$@"
}
