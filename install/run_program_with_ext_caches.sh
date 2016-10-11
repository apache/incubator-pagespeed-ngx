#!/bin/bash
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
