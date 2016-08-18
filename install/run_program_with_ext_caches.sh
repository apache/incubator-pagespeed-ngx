#!/bin/bash
#
# Starts all external cache servers and sets environment appropriately, then
# runs commands in $@ with `eval` builtin and shuts down all servers afterwards.
#
# Example (mind single quotes in the second command so substitution is
# performed inside the script, not when you run it):
#     .../run_program_with_ext_caches.sh \
#       echo Starting client \; \
#       nc localhost '$MEMCACHED_PORT' \; \
#       redis-cli '$REDIS_PORT'

set -e
set -u

# Let's make one script run another. Mind extra single quotes on the second
# line - they are needed to ensure that arguments are expanded in the inner
# script's eval, not outer's. Otherwise there are problems with having \; in
# arguments - it would have been expanded by the first eval.
exec $(dirname "$BASH_SOURCE")/run_program_with_memcached.sh \
  $(dirname "$BASH_SOURCE")/run_program_with_redis.sh \' "$@" \'
