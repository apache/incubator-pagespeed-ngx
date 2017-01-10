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
# Author: morlovich@google.com (Maksim Orlovich)
#
# Helpers for doing experiments with lots of vhosts.
#
# usage:
#   scripts/lots_of_vhosts.sh --config | --traffic
#
# You can also set envvar NUM_VHOSTS to configure how many hosts to use.

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

NUM_VHOSTS=${NUM_VHOSTS:-10000}

function usage {
  cat <<EOF >&2
 Usage:
   scripts/lots_of_vhosts.sh --config | --traffic

   --config generates a suffix for pagespeed.conf
   --traffic generates a list of URLs for trace_stress_test.sh
   You can also set environment variable NUM_VHOSTS to control the number of
   virtual hosts produced.

 See also https://github.com/pagespeed/mod_pagespeed/wiki/Memory-Profiling
EOF
}

function config {
  echo "NameVirtualHost *:8080"
  for i in $(seq 0 $NUM_VHOSTS); do
    echo "<VirtualHost *:8080>"
    echo "  DocumentRoot $HOME/apache2/htdocs/"
    echo "  ServerName vhost"$i".example.com"
    echo "  ModPagespeed on"
    echo "  ModPagespeedFileCachePath \"/tmp/vhost\""
    echo "  ModPagespeedBlockingRewriteKey \"foo"$i"\""
    echo "</VirtualHost>"
  done
}

function traffic {
  for i in $(seq 0 $NUM_VHOSTS); do
    echo "http://vhost"$i".example.com/mod_pagespeed_example/"
  done
}

if [ $# -ne 1 ]; then
  usage
  exit 1
fi

case $1 in
  --config)
    config;;
  --traffic)
    traffic;;
  *)
    usage
    exit 1
    ;;
esac
