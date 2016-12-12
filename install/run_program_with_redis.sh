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
# Checks that that redis is already installed, and then runs it on
# random port with temporary working directory. Port is saved in $REDIS_PORT env
# variable. Commands in $@ (e.g. a test binary) are then run and server is then
# shut down and temporary directory is deleted in the end. These commands are
# run using builtin eval, so be careful and enjoy variable substitutions and
# invoking several commands under same server. It's useful if you want to run
# tests twice: once with cold-cache and once with warm-cache.
#
# Redis is configured to store no more than 1GB of data and will evict some keys
# when it needs more memory.
#
# Example (mind single quotes in the second command so substitution is
# performed inside the script, not when you run it):
#     ../run_program_with_redis.sh \
#       echo Starting client \; \
#       redis-cli '$REDIS_PORT'

set -e
set -u

# Start redis on random port and put log in <workdir>/redis.log
source $(dirname "$BASH_SOURCE")/start_background_server.sh \
  redis-server \
  --bind localhost \
  --port '$SERVER_PORT' \
  --dir '$SERVER_WORKDIR' \
  --logfile 'redis.log' \
  --maxmemory 1gb \
  --maxmemory-policy allkeys-lru
# SERVER_PORT is set by start_background_server.sh, we now want to export it
# under a better name
export REDIS_PORT=$SERVER_PORT

eval "$@"

