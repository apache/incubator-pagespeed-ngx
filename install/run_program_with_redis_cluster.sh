#!/bin/bash
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

function send_command() {
  if $REDIS_CLI -p "$1" | grep -v OK; then
    exit 1
  fi
}

function is_config_consistent() {
  local CONFIGS=()
  for port in "${PORTS[@]}"; do
    # 'CLUSTER NODES' command output is specified here:
    # http://redis.io/commands/cluster-nodes
    # We're choosing following fields: id, ip:port, master/slave, slot-range-1.
    CONFIGS+=("$($REDIS_CLI -p "$port" CLUSTER NODES | \
      awk '{ print $1" "$2" "$4" "$9; }' | \
      sort)")
  done
  for cfg in "${CONFIGS[@]}"; do
    if [[ "${CONFIGS[0]}" != "$cfg" ]]; then
      return 1
    fi
  done
  return 0
}

function is_cluster_healthy() {
  for port in "${PORTS[@]}"; do
    if ! $REDIS_CLI -p "$port" CLUSTER INFO | grep -q "cluster_state:ok"; then
      return 1
    fi
  done
  return 0
}

echo Starting replicas...

# new_redis calls start_background_server.sh, which sets EXIT trap in current
# shell. Traps do not stack, so we have to start each server in a new subshell.
(new_redis; (new_redis; (new_redis; (new_redis; (new_redis; (new_redis; (
  echo -n "Setting up cluster... "
  # Typically Redis Cluster will eventually propagate information itself, but we
  # want to set up the cluster as fast as possible, therefore we make full
  # configuration.
  for a in "${PORTS[@]}"; do
    for b in "${PORTS[@]}"; do
      echo "CLUSTER MEET 127.0.0.1 $b"
    done | send_command $a
  done
  # This configuration should match one in redis_cache_cluster_test.cc, and any
  # changes here should be copied there.
  echo "CLUSTER ADDSLOTS $(seq -s" " 0 5499)" | send_command ${PORTS[0]}
  echo "CLUSTER ADDSLOTS $(seq -s" " 5500 10999)" | send_command ${PORTS[1]}
  echo "CLUSTER ADDSLOTS $(seq -s" " 11000 16383)" | send_command ${PORTS[2]}
  echo "CLUSTER REPLICATE ${IDS[0]}" | send_command ${PORTS[3]}
  echo "CLUSTER REPLICATE ${IDS[1]}" | send_command ${PORTS[4]}
  echo "CLUSTER REPLICATE ${IDS[2]}" | send_command ${PORTS[5]}
  export REDIS_CLUSTER_PORTS="${PORTS[*]}"
  export REDIS_CLUSTER_IDS="${IDS[*]}"
  echo done

  # Although cluster cannot be marked healthy until all slots are covered (e.g.
  # each slave knows all masters), we want all nodes (including slaves) to know
  # about every other node before we start unit tests.
  echo -n "Waiting for configurations to propagate..."
  wait_cmd_with_timeout 3 is_config_consistent
  echo

  # Even if each node received full information about the rest of cluster, it
  # can still wait a little before going into 'healthy' mode.
  echo -n "Waiting for cluster to become healthy..."
  wait_cmd_with_timeout 3 is_cluster_healthy
  echo

  echo Running tests
  "$@"
)))))))

