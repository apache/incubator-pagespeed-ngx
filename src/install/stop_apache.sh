#!/bin/sh
#
# Yet Another Attempt (YAA) at robustly stopping Apache.  Fun facts:
#    - graceful-stop not supported in all versions of Apache 2.2+.
#    - graceful-stop complains about pid in pidfile not running but exits 0
#    - Worker MPM child processes take longer to exit than parent.
#
# Usage:
#     stop_apache.sh apachectl_program pid_file httpd_program stop_command port

set -u

if [ $# != 5 ]; then
  echo Usage: $0 apachectl_program pid_file httpd_program stop_command port
  exit 1
fi

apachectl="$1"
pid_file="$2"
httpd="$(basename $3)"
stop_command="$4"
port="$5"
first=1

$apachectl $stop_command
# Sadly, httpd often misses the memo the first time, so
# we must keep kicking it until it complies.
while [ -f $pid_file ]; do
  if [ $first -eq 1 ]; then
    echo Checking pid file $pid_file ...
    first=0
  else
    sleep 1
  fi

  # '$stop_command' exits with 0 status even if the pid is not
  # running, so we must capture that.
  $apachectl $stop_command | fgrep -q "not running"
  if [ $? -eq 0 ]; then
    rm -f $pid_file
  fi

  # Note: If the above strategy doesn't work out in practice, another
  # approach is to check for the PID being active, e.g.
  #  if ! (ps -p $LOCK_PID | grep -q $LOCK_PID); then
  #    echo "Removing stale lock. Process PID=$LOCK_PID, no longer exists."
  #    rm $LOCKFILE
done

# Even worse with 2.2 worker we have to wait for processes to exit too.
first=1
while [ $(pgrep $httpd|wc -l) -ne 0 ]; do
  if [ $first -eq 1 ]; then
    /bin/echo -n "Waiting for $httpd to exit"
    first=0
  else
    /bin/echo -n "."
  fi
  sleep 1
done

first=1
while [ $(netstat -anp 2>&1 | grep -c "::$port .* LISTEN ") -ne 0 ]; do
  if [ $first -eq 1 ]; then
    /bin/echo -n "Waiting for netstat to stop including refs to port $port"
    first=0
  else
    /bin/echo -n "."
  fi
  sleep 1
done

/bin/echo
