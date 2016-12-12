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
# Checks that redis-server and redis-cli exist and supports Redis Cluster.
# If $REDIS_SERVER/$REDIS_CLI are not set, defaults them to
# redis-server/redis-cli.

function check_redis_server() {
  echo -n "Checking redis-server from '$REDIS_SERVER'... "
  if ! type "$REDIS_SERVER" 2>/dev/null >/dev/null; then
    echo "not found"
    return 1
  fi
  OUTPUT=$("$REDIS_SERVER" --port 0 --cluster-enabled yes 2>&1)
  if echo "$OUTPUT" | grep -q "FATAL CONFIG FILE ERROR"; then
    echo "does not support Redis Cluster"
    return 1
  elif echo "$OUTPUT" | grep -q "Configured to not listen anywhere"; then
    echo "ok"
    return 0
  else
    echo "unknown error"
    return 1
  fi
}

function check_redis_cli() {
  echo -n "Checking redis-cli from '$REDIS_CLI'... "
  if ! type "$REDIS_CLI" 2>/dev/null >/dev/null; then
    echo "not found"
    return 1
  fi
  if "$REDIS_CLI" -v >/dev/null; then
    echo "ok"
    return 0
  else
    echo "unknown error"
    return 1
  fi
}

export REDIS_SERVER=${REDIS_SERVER:-redis-server}
export REDIS_CLI=${REDIS_CLI:-redis-cli}

if ! check_redis_server || ! check_redis_cli; then
  echo -e "\e[31;1mFAIL:\e[0m"
  echo "Looks like you don't have Redis >=3.0, earlier versions do not support"
  echo "Redis Cluster. You can check your version by running 'redis-server -v'."
  echo "Note that some repositories may have old version of redis-server, so"
  echo "you probably want to either skip Redis Cluster tests altogether or"
  echo "build Redis >=3.0 from sources and specify REDIS_SERVER env variable."
  exit 1
fi

