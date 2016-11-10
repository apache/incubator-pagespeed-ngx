#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# This script emulates the behavior of schroot, which:
# - Invokes chroot(2) as root (schroot is setuid).
# - setuids back to the uid it was run as.
# - chdirs to the directory it was run from.
# - execs the command supplied as arguments, or starts an interactive shell.
#
# Trying to write a single command that can be passed to sudo that will do the
# su, chdir, exec combination without breaking arg tokenisation is nigh
# impossible. Instead, once the chrooting and setuiding (via sudo) has been
# taken care of, the script execs itself with --chroot_done. This takes care
# of the chdir and exec.

# This comes from build_env.sh.
if [ -z "${CHROOT_DIR:-}" ]; then
  echo "This must be run via os_redirector.sh!" >&2
  exit 1
fi

# When we re-invoke the script, it's called with:
# --chroot_done <directory> [CMD]
# It then chdirs to <directory> and invokes CMD or an interactive $SHELL
if [ "${1-}" = "--chroot_done" ]; then
  if [ $# -lt 2 ]; then
    echo "Do not run this directly with --chroot_done" >&2
    exit 1
  fi

  cd "$2"
  shift 2

  if [ $# -eq 0 ]; then
    set -- "$SHELL" -l
  fi
  eval exec "$@"
  exit 1  # NOTREACHED
fi

# We need the absolute path to re-exec the script after the chroot.
this_script="$0"
if [[ "$this_script" != /* ]]; then
  this_script="$PWD/$this_script"
fi

# Note that here $0 is expected to be a symlink to os_redirector.sh.
exec setarch i386 sudo /usr/sbin/chroot "$CHROOT_DIR" sudo -u "$USER" -i -- \
  "$this_script" --chroot_done "$PWD" "$@"
