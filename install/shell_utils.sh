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

# Usage: run_with_log [--verbose] <logfile> CMD [ARG ...]
# Runs CMD, writing its output to the supplied logfile. Normally silent
# except on failure, but will tee the output if --verbose is supplied.
function run_with_log() {
  local verbose=
  if [ "$1" = "--verbose" ]; then
    verbose=1
    shift
  fi

  local log_filename="$1"
  shift

  local start_msg="[$(date '+%k:%M:%S')] $@"
  # echo what we're about to do to stdout, including log file location.
  echo "$start_msg >> $log_filename"
  # Now write the same thing to the log.
  echo "$start_msg" >> "$log_filename"

  local rc=0
  if [ -n "$verbose" ]; then
    "$@" 2>&1 | tee -a "$log_filename"
    rc=${PIPESTATUS[0]}
  else
    "$@" >> "$log_filename" 2>&1 || { rc=$?; true; }
  fi
  echo "[$(date '+%k:%M:%S')] Completed with exit status $rc" >> "$log_filename"

  if [ $rc -ne 0 ]; then
    echo
    echo "End of $log_filename:"
    tail "$log_filename"
  fi
  return $rc
}

# Compare version numbers in dotted notation.
# Usage: version_compare <version_a> <comparator> <version_b>
# For instance:
# if version_compare $version -lt 4.2; then echo "Too old!"; fi
#
function version_compare() {
  if [ $# -ne 3 ]; then
    echo "Usage: version_compare <version_a> <comparator> <version_b>" >&2
    exit 1
  fi

  local a=$1
  local comparator=$2
  local b=$3

  if [[ "$a" == *[^.0-9]* ]]; then
    echo "Non-numeric version: $a" >&2
    exit 1
  fi

  if [[ "$b" == *[^.0-9]* ]]; then
    echo "Non-numeric version: $b" >&2
    exit 1
  fi

  # The computed difference. 0 means a == b, -1 means a < b, 1 means a > b.
  local difference=0

  while [ $difference -eq 0 ]; do
    if [ -z "$a" -a -z "$b" ]; then
      break
    elif [ -z "$a" ]; then
      # a="" and b != "", therefore a < b
      difference=-1
      break
    elif [ -z "$b" ]; then
      # a != "" and b="", therefore a > b
      difference=1
      break
    fi

    # $a is N[.N.N]. Extract the first N from the beginning into $a_tok
    local a_tok="${a%%.*}"
    # Make $a any remaining N.N.
    a="${a#*.}"
    [ "$a" = "$a_tok" ] && a=""  # Happens when there are no dots in $a

    # Same for $b
    local b_tok="${b%%.*}"
    b="${b#*.}"
    [ "$b" = "$b_tok" ] && b=""

    # Now do the integer comparison between a and b.
    if [ "$a_tok" -lt "$b_tok" ]; then
      difference=-1
    elif [ "$b_tok" -gt "$b_tok" ]; then
      difference=1
    fi
  done

  # Now do the actual comparison. We use the supplied comparator (say -le) to
  # compare the computed difference with zero. This will return the expected
  # result.
  [ "$difference" "$comparator" 0 ]
}
