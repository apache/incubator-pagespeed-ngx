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
start_test Handler access restrictions
function expect_handler() {
  local host_prefix="$1"
  local handler="$2"
  local expectation="$3"

  URL="http://$host_prefix.example.com/$handler"
  echo "http_proxy=$SECONDARY_HOSTNAME curl $URL"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME \
    $CURL -o/dev/null -sS --write-out '%{http_code}\n' "$URL")
  if [ "$expectation" = "allow" ]; then
    check [ "$OUT" = "200" ]
  elif [ "$expectation" = "deny" ]; then
    check [ "$OUT" = "403" -o "$OUT" = "404" ]
  else
    check false
  fi
}
function expect_messages() {
  expect_handler "$1" "$MESSAGES_HANDLER" "$2"
}

# Listed at top level.
expect_messages messages-allowed allow
# Listed at top level.
expect_messages more-messages-allowed allow
# Not listed at any level.
expect_messages messages-still-not-allowed deny
# Listed at VHost level.
expect_messages but-this-message-allowed allow
# Listed at VHost level.
expect_messages and-this-one allow
# Listed at top level, VHost level lists CLEAR_INHERITED.
expect_messages cleared-inherited deny
# Listed at top level, VHost level lists both this and CLEAR_INHERITED.
expect_messages cleared-inherited-reallowed allow
# Not listed at top level, VHost level lists both this and CLEAR_INHERITED.
expect_messages messages-allowed-at-vhost allow
# Not listed at any level, VHost level lists only CLEAR_INHERITED.
expect_messages cleared-inherited-unlisted allow
# Not listed at any level, VHost level lists CLEAR_INHERITED and some other
# domains.
expect_messages messages-not-allowed-at-vhost deny
# Listed at top level, via wildcard.
expect_messages anything-a-wildcard allow
# Listed at top level, via wildcard.
expect_messages anything-b-wildcard allow
# Listed at top level, via wildcard, VHost level lists CLEAR_INHERITED.
expect_messages anything-c-wildcard deny
# VHost lists deny *
expect_messages nothing-allowed deny
# Not listed at any level.
expect_messages messages-not-allowed deny
# Listed at top level, VHost level lists CLEAR_INHERITED.
expect_messages cleared-inherited deny

# No <Handler>Domains listings for these, default is allow.
expect_handler nothing-explicitly-allowed $STATISTICS_HANDLER allow
expect_handler nothing-explicitly-allowed $GLOBAL_STATISTICS_HANDLER allow
expect_handler nothing-explicitly-allowed pagespeed_admin/ allow
expect_handler nothing-explicitly-allowed pagespeed_global_admin/ allow
expect_handler nothing-explicitly-allowed pagespeed_console allow

# Listed at VHost level as allowed.
expect_handler everything-explicitly-allowed $STATISTICS_HANDLER allow
expect_handler everything-explicitly-allowed $GLOBAL_STATISTICS_HANDLER allow
expect_handler everything-explicitly-allowed pagespeed_admin/ allow
expect_handler everything-explicitly-allowed pagespeed_global_admin/ allow
expect_handler everything-explicitly-allowed pagespeed_console allow

# Other domains listed at VHost level as allowed, none of these listed.
expect_handler everything-explicitly-allowed-but-aliased \
  $STATISTICS_HANDLER deny
expect_handler everything-explicitly-allowed-but-aliased \
  $GLOBAL_STATISTICS_HANDLER deny
expect_handler everything-explicitly-allowed-but-aliased \
  pagespeed_admin/ deny
expect_handler everything-explicitly-allowed-but-aliased \
  pagespeed_global_admin/ deny
expect_handler everything-explicitly-allowed-but-aliased \
  pagespeed_console deny
