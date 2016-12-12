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
# Rotate the logs in the apache logs directory specified on the command line,
# and gzip them, then erase old logs if disk usage is over 85%.  Note that
# apache must be stopped when we do this.  Note also that we take pains not to
# erase the newly-rotated logs, as those are the ones we are likely to care
# deeply about.  This may mean the disk stays full, but logs are pretty
# compressible so it's unlikely.
set -e
now() {
  date '+%Y%m%d-%H%M'
}
stamp=$(now)
if [ $# -ne 1 -o ! -d "$1" ]; then
  echo "Usage: apache_rotate_logs.sh logs_directory" >&2
  exit 1
fi
cd "$1"
if [ -f "error_log.gz" ]; then
  # Clean up after partial log rotation
  echo "Cleaning up error_log.gz"
  mv error_log.gz error_log.$stamp.gz
  cleaned_up=true
fi
if [ -f "access_log.gz" ]; then
  # Clean up after partial log rotation
  echo "Cleaning up access_log.gz"
  mv access_log.gz access_log.$stamp.gz
  cleaned_up=true
fi
if [ ! -f "error_log" -a ! -f "access_log" ]; then
  # No logs to rotate.
  echo "No new logs to rotate"
else
  # gzip can be kind of slow, so parallelize.
  # But gzip well, as this stuff eats a ton of space.
  if [ -f "error_log" ]; then
    echo "Gzipping error_log"
    gzip -9 error_log &
  fi
  if [ -f "access_log" ]; then
    echo "Gzipping access_log"
    gzip -9 access_log
  fi
  wait
  if [ ! -z "$cleaned_up" ]; then
    # If we used stamp, create a fresh one (effectively spin)
    old_stamp=stamp
    stamp=$(now)
    while [ "$stamp" == "$old_stamp" ]; do
      sleep 1
      stamp=$(now)
    done
  fi
  # Now timestamp the just-compressed logs.
  if [ -f "error_log.gz" ]; then
    echo "Timestamping error_log"
    mv error_log.gz error_log.$stamp.gz
  fi
  if [ -f "access_log.gz" ]; then
    echo "Timestamping access_log"
    mv access_log.gz access_log.$stamp.gz
  fi
fi
# Clean up old logs if the disk is getting full (>85%).
df_percent() {
  df . --output=pcent | egrep -o '[0-9]+'
}
if [ $(df_percent) -ge 85 ]; then
  echo "Cleaning required."
  for log in $(/bin/ls -1tr *_log.[0-9]* | head -n -2); do
    echo "Cleaning $log"
    rm $log
    if [ $(df_percent) -lt 85 ]; then
      break
    fi
  done
fi
