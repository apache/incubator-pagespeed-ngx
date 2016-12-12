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
# Sets up Redis Cluster and runs corresponding unit tests.
# TODO(yeputons) it does not know what to do in open-source yet

set -e
set -u

source $(dirname "$BASH_SOURCE")/shell_utils.sh || exit 1
source $(dirname "$BASH_SOURCE")/find_redis_cluster.sh || exit 1

BUILDTYPE=${BUILDTYPE:-Release}
PORTS=()
IDS=()
function new_redis() {
  # Start redis on random port and put log in <workdir>/redis.log
  source "$(dirname "$BASH_SOURCE")/start_background_server.sh" \
    "$REDIS_SERVER" \
    --bind localhost \
    --port '$SERVER_PORT' \
    --dir '$SERVER_WORKDIR' \
    --logfile 'redis.log' \
    --maxmemory 1gb \
    --maxmemory-policy allkeys-lru \
    --cluster-node-timeout 1000 \
    --cluster-enabled yes
  PORTS+=("$SERVER_PORT")
  ID=$($REDIS_CLI -p $SERVER_PORT CLUSTER NODES | awk '{print $1}')
  [[ -n "$ID" ]] || exit 1
  IDS+=("$ID")
}

echo Starting replicas...

# new_redis calls start_background_server.sh, which sets EXIT trap in current
# shell. Traps do not stack, so we have to start each server in a new subshell.
(new_redis; (new_redis; (new_redis; (new_redis; (new_redis; (new_redis; (
  export REDIS_CLUSTER_PORTS="${PORTS[*]}"
  export REDIS_CLUSTER_IDS="${IDS[*]}"

  echo Running redis_cache_cluster_setup...
  SETUP_DIR=out/$BUILDTYPE
  if [ ! -e $SETUP_DIR ]; then                                        # [google]
    SETUP_DIR=$TEST_SRCDIR/google3/third_party/pagespeed/system/      # [google]
  fi                                                                  # [google]
  $SETUP_DIR/redis_cache_cluster_setup || exit 1
  echo done

  echo Running tests
  "$@"
)))))))

