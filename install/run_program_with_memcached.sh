#!/bin/bash
#
# Copyright 2014 Google Inc.
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
# Checks that that memcached is already installed, and then runs it on
# random port with temporary working directory. Port is saved in $MEMCACHED_PORT
# env variable. Commands in $@ (e.g. a test binary) are then run and server is
# then shut down and temporary directory is deleted in the end. These commands
# are run using builtin eval, so be careful and enjoy variable substitutions and
# invoking several commands under same server. It's useful if you want to run
# tests twice: once with cold-cache and once with warm-cache.
#
# Example (mind single quotes in the second command so substitution is
# performed inside the script, not when you run it):
#     .../run_program_with_memcached.sh \
#       echo Starting client \; \
#       echo nc localhost '$MEMCACHED_PORT'

set -e
set -u

# If memcached is run as root, it expects an explicit -u user argument, or
# it will refuse to start. Normally one would want to use a restricted
# user, but for integration tests, root will do.
MEMCACHED_USER_OPTS=""
if [[ "$UID" == "0" ]]; then
  MEMCACHED_USER_OPTS="-u root"
fi

# Mind single quotes around '>log' because we redirect server log only, not
# whole script output. This redirection is processed by eval inside.
source $(dirname "$BASH_SOURCE")/start_background_server.sh \
  memcached \
  -l localhost \
  -p '$SERVER_PORT' \
  -U 0 \
  -m 1024 \
  $MEMCACHED_USER_OPTS \
  '>$SERVER_WORKDIR/memcached.log'
# SERVER_PORT is set by start_background_server.sh, we now want to export it
# under better name
export MEMCACHED_PORT=$SERVER_PORT

eval "$@"

