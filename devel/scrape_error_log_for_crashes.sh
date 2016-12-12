#!/bin/bash
#
# Copyright 2012 Google Inc.
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

if [ $# -lt 2 ]; then
  echo Usage: $0 error_log_filename stop_filename
  exit 1
fi

error_log="$1"
stop_file="$2"

(tail -f $error_log | egrep "exit signal|CRASH") & background_pid=$!
while [ ! -e "$stop_file" ]; do sleep 10; done
kill $background_pid

rm -f "$stop_file"
