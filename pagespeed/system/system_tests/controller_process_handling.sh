#!/bin/bash
#
# Copyright 2016 Google Inc.
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
function get_controller_pid() {
  grep "Controller running with PID " $ERROR_LOG | tail -n 1 | awk '{print $NF}'
}
if [ "$RUN_CONTROLLER_TEST" = "on" ]; then
  start_test babysitter process restarts controller when killed

  controller_pid=$(get_controller_pid)
  check [ ! -z "$controller_pid" ]  # Controller PID should be in log.

  # We should see the babysitter process starting too.
  check grep "Babysitter running with PID " $ERROR_LOG

  function count_watcher_messages() {
    grep -c "Watching the root process to exit if it dies." $ERROR_LOG
  }

  # And the ProcessDeathWatcherThread should be running.
  echo "Checking that we're watching the right processes."
  initial_watcher_count=$(count_watcher_messages)
  # On nginx this will be 1; on apache it will be 2 because apache starts twice to
  # check its config.
  check [ $initial_watcher_count -gt 0 ]

  # Now kill the controller and verify that it gets restarted.
  kill "$controller_pid"

  function did_controller_restart() {
    new_controller_pid=$(get_controller_pid)

    # If there's a new PID, that means it was restarted.
    test ! -z "$new_controller_pid" -a "$new_controller_pid" != "$controller_pid"
  }

  echo -n "Waiting for babysitter to restart controller ..."
  SECONDS=0
  while ! did_controller_restart && [ $SECONDS -lt 10 ]; do
    echo -n .
    sleep 0.1
  done
  echo

  check did_controller_restart

  echo "Checking that babysitter reported controller death..."
  grep "Controller process $controller_pid exited with wait status" \
    $ERROR_LOG > /dev/null

  # The ProcessDeathWatcherThread should have been restarted (it's hosted by the
  # controller thread, not the babysitter). This message may be delayed slightly
  # under valgrind, so allow a few retries.
  echo "Checking again that we're watching the right processes."
  final_watcher_count=$(count_watcher_messages)
  SECONDS=0
  while [ $final_watcher_count -eq $initial_watcher_count -a\
          $SECONDS -lt 2 ]; do
    sleep 0.1
    final_watcher_count=$(count_watcher_messages)
  done
  check [ $final_watcher_count -eq $((initial_watcher_count + 1)) ]
elif [ "$FIRST_RUN" = "true" ]; then
  start_test With controller off, there should be no pid.
  # This should only be checked in the first frun because there may
  # be a leftover pid from an earlier run in the error.log.  Related:
  # we must ensure that whenever FIRST_RUN is true, the logs must
  # be cleared before running the test script.
  check [ "$(get_controller_pid)" = "" ];
fi
