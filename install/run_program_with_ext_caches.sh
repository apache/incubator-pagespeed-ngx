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
# Starts all external cache servers and sets environment appropriately, then
# runs single command in $@. There should be no substitutions in $@, as they may
# happen in between starts of different servers.
#
# Typically this should be used to run single executables with some parameters.

set -e

if [ ! -z "$DISABLE_EXT_CACHES" ]; then
  exec "$@"
fi

set -u

# Let's make one script run another. We really do not want to deal with proper
# escaping of quotes and bash commands on three levels.
exec $(dirname "$BASH_SOURCE")/run_program_with_memcached.sh \
  $(dirname "$BASH_SOURCE")/run_program_with_redis.sh \
    $(dirname "$BASH_SOURCE")/run_program_with_redis_cluster.sh "$@"
