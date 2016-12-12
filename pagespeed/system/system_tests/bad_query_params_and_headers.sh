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
start_test Accept bad query params and headers

# The examples page should have this EXPECTED_EXAMPLES_TEXT on it.
EXPECTED_EXAMPLES_TEXT="PageSpeed Examples Directory"
OUT=$(wget -O - $EXAMPLE_ROOT)
check_from "$OUT" fgrep -q "$EXPECTED_EXAMPLES_TEXT"

# It should still be there with bad query params.
OUT=$(wget -O - $EXAMPLE_ROOT?PageSpeedFilters=bogus)
check_from "$OUT" fgrep -q "$EXPECTED_EXAMPLES_TEXT"

# And also with bad request headers.
OUT=$(wget -O - --header=PageSpeedFilters:bogus $EXAMPLE_ROOT)
check_from "$OUT" fgrep -q "$EXPECTED_EXAMPLES_TEXT"
