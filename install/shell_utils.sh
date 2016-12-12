#!/bin/bash
#
# Copyright 2011 Google Inc.
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

GIT_VERSION=2.0.4
GIT_SHA256SUM=dd9df02b7dcc75f9777c4f802c6b8562180385ddde4e3b8479e079f99cd1d1c9

WGET_VERSION=1.12
WGET_SHA256SUM=7578ed0974e12caa71120581fa3962ee5a69f7175ddc3d6a6db0ecdcba65b572

MEMCACHED_VERSION=1.4.20
MEMCACHED_SHA256SUM=25d121408eed0b1522308ff3520819b130f04ba0554c68a673af23a915a54018

PYTHON_VERSION=2.7.8
PYTHON_SHA256SUM=74d70b914da4487aa1d97222b29e9554d042f825f26cb2b93abd20fdda56b557

REDIS_VERSION=3.2.4
REDIS_SHA256SUM=2ad042c5a6c508223adeb9c91c6b1ae091394b4026f73997281e28914c9369f1

GIT_SRC_URL=https://kernel.org/pub/software/scm/git/git-$GIT_VERSION.tar.gz
WGET_SRC_URL=https://ftp.gnu.org/gnu/wget/wget-$WGET_VERSION.tar.gz
# This is available on https, but CentOS 6's wget doesn't like the cert.
MEMCACHED_SRC_URL=http://www.memcached.org/files/memcached-$MEMCACHED_VERSION.tar.gz
PYTHON_SRC_URL=https://www.python.org/ftp/python/$PYTHON_VERSION/Python-$PYTHON_VERSION.tgz
REDIS_SRC_URL=http://download.redis.io/releases/redis-$REDIS_VERSION.tar.gz

# Usage: install_from_src [package] [...]
# For all supplied package names, downloads and installs from source.
function install_from_src() {
  local pkg
  for pkg in "$@"; do
    if [ -e "/usr/local/bin/$pkg" ]; then
      echo "$pkg already installed, will not re-install"
      continue
    fi

    case "$pkg" in
      git) [ "$(lsb_release -is)" = "CentOS" ] && yum -y install curl-devel;
           install_src_tarball $GIT_SRC_URL $GIT_SHA256SUM ;;
      memcached) install_src_tarball $MEMCACHED_SRC_URL $MEMCACHED_SHA256SUM ;;
      python2.7)
        install_src_tarball $PYTHON_SRC_URL $PYTHON_SHA256SUM altinstall
        # On Centos5, yum needs /usr/bin/python to be 2.4 but gclient needs
        # python on the path to be 2.6 or later.
        if [ "$(lsb_release -is)" = "CentOS" ] && \
           version_compare "$(lsb_release -rs)" -lt 6; then
          for dir in $HOME ~$SUDO_USER; do
            mkdir -p $dir/bin && ln -sf /usr/local/bin/python2.7 $dir/bin/python
          done
        fi ;;
      wget) install_src_tarball $WGET_SRC_URL $WGET_SHA256SUM ;;
      redis-server) install_src_tarball $REDIS_SRC_URL $REDIS_SHA256SUM ;;
      *) echo "Internal error: Unknown source package: $pkg" >&2; return 1 ;;
    esac
  done
}

# Usage: install_src_tarball <tarball_url> [install_target]
# Downloads the supplied tarball, builds and installs the contents.
# If install_target is supplied it will be used instead of "make install".
function install_src_tarball() {
  if [ $# -lt 2 -o $# -gt 3 ]; then
    echo "Usage: install_src_tarball <tarball> <sha256sum> [install_target]" >&2
    exit 1
  fi

  local url="$1"
  local expected_256sum="$2"
  local install_target="${3:-install}"
  local filename="$(basename "$url")"
  local dirname="$(basename "$filename" .tar.gz)"
  dirname="$(basename "$dirname" .tgz)"

  local tmpdir="$(mktemp -d)"
  pushd "$tmpdir"
  # CentOS 5 can't fetch from the git repo because it has an ancient OpenSSL,
  # so if a file has been scp'd manually, use it.
  if [ -e "$HOME/$filename" ]; then
    filename="$HOME/$filename"
  else
    wget "$url"
  fi

  local actual_256sum="$(sha256sum "$filename" | cut -d ' ' -f 1)"
  if [ "$actual_256sum" != "$expected_256sum" ]; then
    echo "sha256sum mismatch on $filename." >&2
    echo "Expected $expected_256sum got $actual_256sum" >&2
    exit 1
  fi

  # There's no error handling here because we ought to be running under set -e.
  tar -xf "$filename"
  cd "$dirname"
  if [ -e ./configure ]; then
    ./configure
  fi
  make
  echo "Installing $dirname"
  sudo make $install_target
  popd
  rm -rf "$tmpdir"
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
  mkdir -p "$(dirname "$log_filename")"
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
    if [ -n "${TRAVIS:-}" ]; then
      # Travis has a 4MB total log output limit. -c 3000 is ~3MB.
      tail -c 3000 "$log_filename"
    else
      tail -n 20 "$log_filename"
    fi
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
