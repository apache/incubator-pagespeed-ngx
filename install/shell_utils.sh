# Copyright 2011 Google Inc. All Rights Reserved.
# Author: sligocki@google.com (Shawn Ligocki)
#
# Common shell utils.

# Usage: kill_prev PORT
# Kill previous processes listening to PORT.
function kill_prev() {
  echo -n "Killing anything that listens on 0.0.0.0:$1... "
  local pids=$(lsof -w -n -i "tcp:$1" -s TCP:LISTEN -Fp | sed "s/^p//" )
  if [[ "${pids}" == "" ]]; then
    echo "no processes found";
  else
    kill -9 ${pids}
    echo "done"
  fi
}

# Usage: wait_cmd CMD [ARG ...]
# Wait for a CMD to succeed. Tries it 10 times every 0.1 sec.
# That maxes to 1 second if CMD terminates instantly.
function wait_cmd() {
  for i in $(seq 10); do
    if eval "$@"; then
      return 0
    fi
    sleep 0.1
  done
  eval "$@"
}

# Usage: wait_cmd_with_timeout TIMEOUT_SECS CMD [ARG ...]
# Waits until CMD succeed or TIMEOUT_SECS passes, printing progress dots.
# Returns exit code of CMD. It works with CMD which does not terminate
# immediately.
function wait_cmd_with_timeout() {
  # Bash magic variable which is increased every second. Note that assignment
  # does not reset timer, only counter, i.e. it's possible that it will become 1
  # earlier than after 1s.
  SECONDS=0
  while [[ "$SECONDS" -le "$1" ]]; do  # -le because of measurement error.
    if eval "${@:2}"; then
      return 0
    fi
    sleep 0.1
    echo -n .
  done
  eval "${@:2}"
}
