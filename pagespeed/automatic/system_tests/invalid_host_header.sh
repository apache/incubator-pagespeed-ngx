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
start_test Invalid HOST URL does not crash the server.

if [[ "$HOSTNAME" == *:* ]]; then
  host=$(echo $HOSTNAME | cut -d : -f 1)
  port=$(echo $HOSTNAME | cut -d : -f 2)
else
  host=$HOSTNAME
  port=80
fi

exec 3<>/dev/tcp/$host/$port
echo -e "GET /mod_pagespeed_example/ HTTP/1.1\nHost: 127.0.0.\xEF\xBF\xBD\n" >&3
# Read first line of HTTP response with a timeout of 1 second.
# It would be nice to get the whole body with:
# OUT="$(timeout 1 cat <&3)", but CentOS 5 lacks the timeout command.
read -t 1 OUT <&3

# Expect a 200 response and not, say, EOF.
check_from "$OUT" grep -q "HTTP/1.[01] 200 OK"
