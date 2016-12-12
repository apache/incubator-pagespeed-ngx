#!/bin/bash
#
# Copyright 2012 Google Inc.
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
# Note: this script is not yet usable outside Google, because it depends on a
# corpus database that we can't open source.  It should be possible to create a
# db with a combination of mod_pagespeed's slurping and a headless browser, but
# we don't currently have a script or instructions on how to do this.
# TODO(jefftk): resolve this
#
# This script runs a mod_pagespeed load-test.  The typical
# configuration is to run this on your development workstation and
# mps_generate_load.sh will be run (via ssh) on a different machine
# (localhost by default, for single-machine runs).
#
# Usage:  scripts/mps_load_test.sh
#             [-start_apache_then_exit]
#             [-custom_so mod_pagespeed.so]
#             [-custom_so24 mod_pagespeed_ap24.so]
#             [-user_agent user_agent_string]
#             [-chrome]
#             [-memcached|-redis]
#             [-ipro_preserve]
#             [-purging]
#             [-inline_unauthorized_resources]
#             [-ssl]
#             [-debug]
#             [corpus_file.tar.bz2]
#
# Note: Order of supplied command line parameters matters for correct working.
#
# corpus_file.tar.bz2 is mandatory on the first run and ignored on later runs.
# The extracted version is stored in the directory specified by $corpus (see
# below) between runs.
#
# Example of user_agent_string: Chrome/23.0.1271.17
# Saying '-chrome' is equivalent to saying 'give me a recent version of Chrome,
# which needs to be updated by editing this script.
#
# If the 'machine name' argument is "localhost" then:
#   - you will not be prompted for your ssh password
#   - your machine will be unusable for a little while
#   - your results may be more consistent
#
# This scripts prompts you for your su password if it needs to set your
# /proc/sys/net/ipv4/tcp_tw_recycle file to contain a "1".

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

this_dir=$(dirname "${BASH_SOURCE[0]}")
cd "$this_dir/.."
src="$PWD"

gen_load="$src/devel/mps_generate_load.sh"
corpus="$HOME/pagespeed-loadtest-corpus"

start_apache_then_exit=0

# Check if we are asked for an external cache (e.g memcached or redis) but
# don't have a port configured first, as we need to re-launch ourselves using
# run_program_with_EXTCACHE.sh, so we don't want to mess up $@.
for argument in "$@"; do
  if [ "$argument" = "-memcached" -a -z "${MEMCACHED_PORT+x}" ]; then
    exec "$src/install/run_program_with_memcached.sh" \
        "$src/devel/mps_load_test.sh" "$@"
  elif [ "$argument" = "-redis" -a -z "${REDIS_PORT+x}" ]; then
    exec "$src/install/run_program_with_redis.sh" \
        "$src/devel/mps_load_test.sh" "$@"
  fi
done

if [[ $# -ge 1 && "$1" = "-start_apache_then_exit" ]]; then
  start_apache_then_exit=1
  shift
fi

custom_so=
if [[ $# -ge 2 && "$1" = "-custom_so" ]]; then
  custom_so=$2
  shift 2
fi

custom_so24=
if [[ $# -ge 2 && "$1" = "-custom_so24" ]]; then
  custom_so24=$2
  shift 2
fi

if [[ $# -ge 1 && "$1" = "-chrome" ]]; then
  export USER_AGENT_FLAG="--user_agent Chrome/47.0.2526.80"
  shift
elif [[ $# -ge 2 && "$1" = "-user_agent" ]]; then
  export USER_AGENT_FLAG="--user_agent $2"
  shift 2
else
  export USER_AGENT_FLAG=
fi

if [[ $# -ge 1 && "$1" = "-memcached" ]]; then
  echo Using memcached on port $MEMCACHED_PORT
  shift
  export MEMCACHED=1
  export REDIS=0
  cache_stat_prefix="memcache"
elif [[ $# -ge 1 && "$1" = "-redis" ]]; then
  echo Using redis on port "$REDIS_PORT"
  shift
  export MEMCACHED=0
  export REDIS=1
  cache_stat_prefix="redis"
else
  export MEMCACHED=0
  export REDIS=0
  cache_stat_prefix="file_cache_"
fi

if [[ $# -ge 1 && "$1" = "-ipro_preserve" ]]; then
  shift
  export IPRO_PRESERVE=1
  export EXTRA_URL_FLAGS=--ipro_preserve
else
  export IPRO_PRESERVE=0
  export EXTRA_URL_FLAGS=
fi

if [[ $# -ge 1 && "$1" = "-purging" ]]; then
  shift
  export PURGING=1
else
  export PURGING=0
fi

if [[ $# -ge 1 && "$1" = "-inline_unauthorized_resources" ]]; then
  shift
  export IUR=1
else
  export IUR=0
fi

if [[ $# -ge 1 && "$1" = "-ssl" ]]; then
  shift
  export EXTRA_FETCH_FLAGS=--ssl
else
  export EXTRA_FETCH_FLAGS=
fi

if [[ $# -ge 1 && "$1" = "-debug" ]]; then
  shift
  compile_mode="Debug"
else
  compile_mode="OptDebug"
fi

if [ $# -ge 2 ]; then
  echo "Unknown arguments: $@"
  exit 1
fi

if [ ! -d "$corpus" ] && [ $# -ne 1 ]; then
  echo "Invalid arguments: corpus required"
  exit 1
fi

if [ -d "$corpus" ] && [ $# -ne 0 ]; then
  echo "Warning: using already extracted corpus instead of $1"
fi

# If an 'su' password is required, then get it before going off and compiling
# stuff.
"$src/devel/turn_on_timewait_recyling.sh"

if [ -d /var/run/pagespeed/ ]; then
  rm -rf /var/run/pagespeed/*
else
  sudo mkdir -p /var/run/pagespeed
  sudo chown $USER /var/run/pagespeed
fi

# Only ssh (and warn user that they will need a password) if using a separate
# host for load generation.
cmd="$gen_load $EXTRA_URL_FLAGS $EXTRA_FETCH_FLAGS $USER_AGENT_FLAG"

APACHE_DEBUG_ROOT=${APACHE_DEBUG_ROOT:-$HOME/apache2}

echo Checking whether we have the corpus available.
if [ ! -d "$corpus" ]; then
  corpus_src="$1"
  echo "Copying corpus files from $corpus_src"
  mkdir -p "$corpus"
  cd "$corpus"
  tar xjf "$corpus_src"
fi

cd "$src/devel"

# Build a version of mod_pagespeed with all optimizations enabled, but with
# a build that includes DCHECKs.
make -j8 CONF=$compile_mode apache_trace_stress_test_server \
  DUMP_DIR="$corpus" \
  APACHE_DEBUG_ROOT=${APACHE_DEBUG_ROOT} \
  MOD_PAGESPEED_CACHE=/var/run/pagespeed/cache

# If a custom .so got specified, install it.
if [[ -n "$custom_so" ]]; then
  install -c $custom_so /usr/local/apache2/modules/mod_pagespeed.so
fi

if [[ -n "$custom_so24" ]]; then
  install -c $custom_so24 /usr/local/apache2/modules/mod_pagespeed_ap24.so
fi

# Restart apache for any hand-specified .so or alternative binary
if [[ -n "$custom_so" || -n "$custom_so24" ]]; then
  make apache_debug_stop
  make apache_debug_start
fi

if [[ "$start_apache_then_exit" = 1 ]]; then
  exit 0
fi

stop_crash_scraper="/tmp/stop_crash_scraper"
error_log="$APACHE_DEBUG_ROOT/logs/error_log"
rm -f "$stop_crash_scraper"

echo starting test ...
"$src/devel/scrape_error_log_for_crashes.sh" \
    "$error_log" "$stop_crash_scraper" &
echo $cmd ...
$cmd
touch "$stop_crash_scraper"

# Print some interesting statistics from the server
statsfile=/tmp/mps_load_test_stats.$$
wget -q -O $statsfile http://localhost:8080/mod_pagespeed_global_statistics
grep "$cache_stat_prefix" $statsfile | grep -v onchange=
grep shm $statsfile
grep dropped $statsfile
grep cache_batcher $statsfile
grep rewrite_cached_output_missed_deadline $statsfile
grep bytes_saved $statsfile
grep serf $statsfile
grep queued-fetch-count $statsfile
grep page_load_count $statsfile
grep 404_count $statsfile
grep file_cache_bytes_freed_in_cleanup $statsfile
grep file_cache_cleanups $statsfile
grep file_cache_write_errors $statsfile
grep image_webp_rewrites $statsfile
egrep "num_css|num_js" $statsfile
rm -f $statsfile

set +e
echo 'egrep "exit signal|CRASH" $error_log'
egrep "exit signal|CRASH" $error_log
if [ $? = 0 ]; then
  echo "*** $error_log has dangerous looking errors.  Please investigate."
  exit 1
else
  echo "No deaths reported in $error_log -- ship it."
fi

if [ "$MEMCACHED" = "1" -o "$REDIS" = "1" ]; then
  date
  echo -n Sleeping 5 seconds before killing external cache server to let
  echo -n outstanding writes quiesce...
  sleep 5
  echo done
fi
