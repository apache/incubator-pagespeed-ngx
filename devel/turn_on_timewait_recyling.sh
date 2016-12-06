#!/bin/bash
#
# Makes linux enable fast reuse of sockets in TIME-WAIT state.  We need this for
# load testing so that we don't run out of connection table slots and fail to
# get a socket.  This isn't generally a good idea to set on public-facing
# servers because of trouble with NATs, but is fine here.
#
# This scripts prompts you for your su password if TIME-WAIT recycling isn't
# already enabled.

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

recycle_file=/proc/sys/net/ipv4/tcp_tw_recycle
if [ $(cat $recycle_file) -ne 1 ]; then
  echo "Putting a '1' in proc/sys/net/ipv4/tcp_tw_recycle to avoid"
  echo "running out of port numbers (due to old ones being in TIME_WAIT state)."
  echo 1 >/tmp/1
  sudo cp /tmp/1 $recycle_file
  rm /tmp/1
fi
