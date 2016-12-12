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
start_test IPRO requests are routed through the controller API

STATS=$OUTDIR/controller_stats
$WGET_DUMP $GLOBAL_STATISTICS_URL > $STATS.0

OUT=$($WGET_DUMP $TEST_ROOT/ipro/instant/wait/purple.css?random=$RANDOM)
check_from "$OUT" fgrep -q 'body{background:#9370db}'

$WGET_DUMP $GLOBAL_STATISTICS_URL > $STATS.1
check_stat $STATS.0 $STATS.1 named-lock-rewrite-scheduler-granted 0
check_stat $STATS.0 $STATS.1 popularity-contest-num-rewrites-succeeded 1
