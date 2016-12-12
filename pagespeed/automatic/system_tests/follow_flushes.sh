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
if [ -z "${DISABLE_PHP_TESTS:-}" ]; then
  start_test Follow flushes does what it should do.
  echo "Check that FollowFlushes on outputs timely chunks"
  # The php file will write 6 chunks, but the last two often get aggregated
  # into one. Hence 5 or 6 is what we want to see.
  check_flushing flush 2.2 5
fi

