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
# Test if the warning messages are colored in message_history page.
# We color the messages in message_history page to make it clearer to read.
# Red for Error messages. Brown for Warning messages.
# Orange for Fatal messages. Black by default.
# Won't test Error messages and Fatal messages in this test.
start_test Messages are colored in message_history
INJECT=$($CURL --silent $HOSTNAME/?PageSpeed=Warning_trigger)
OUT=$($WGET -q -O - $HOSTNAME/pagespeed_admin/message_history | \
  grep Warning_trigger)
check_from "$OUT" fgrep -q "color:brown;"
