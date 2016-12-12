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
# This script takes the output of trace_stress_test.sh and reports cheap and
# cheerful median, 75th, 90th, 95th, 99th, and worst latencies.
if [ "X$1" == "X" ]; then
  # work from stdin
  sorted="/tmp/latency-$$-sorted.txt"
  sort -r -g -k 1 > $sorted
else
  # construct sorted file name
  sorted="${1%%.txt}-sorted.txt"
  sort -r -g -k 1 "$1" > "$sorted"
fi
echo "Sorted latency data in: $sorted" 1>&2
echo "%  ms            status url" 1>&2
lines=$(wc -l < "$sorted")
for i in 50 75 90 95 99; do
  divisor=$((100 / (100 - $i)))
  echo -n "$i "
  head -$(($lines / $divisor)) "$sorted" | tail -1
done
echo -n "mx "
head -1 "$sorted"
