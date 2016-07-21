#!/bin/bash
#
# Checks that that redis is already installed, and then runs it on
# random port with temporary working directory. Port is saved in $REDIS_PORT env
# variable. Commands in $@ (e.g. a test binary) are then run and server is then
# shut down and temporary directory is deleted in the end. These commands are
# run using builtin eval, so be careful and enjoy variable substitutions and
# invoking several commands under same server. It's useful if you want to run
# tests twice: once with cold-cache and once with warm-cache.
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
  --maxmemory 1000000000
# SERVER_PORT is set by start_background_server.sh, we now want to export it
# under a better name
export REDIS_PORT=$SERVER_PORT

eval "$@"

