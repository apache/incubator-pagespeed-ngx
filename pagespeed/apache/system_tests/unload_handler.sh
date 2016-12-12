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
  start_test add_instrumentation has added unload handler with \
    ModPagespeedReportUnloadTime enabled in APACHE_SECONDARY_PORT.
URL="$SECONDARY_TEST_ROOT/add_instrumentation.html\
?PageSpeedFilters=add_instrumentation"
echo http_proxy=$SECONDARY_HOSTNAME $WGET -O $WGET_OUTPUT $URL
http_proxy=$SECONDARY_HOSTNAME $WGET -O $WGET_OUTPUT $URL
check [ $(grep -o "<script" $WGET_OUTPUT|wc -l) = 3 ]
check [ $(grep -c "pagespeed.addInstrumentationInit('/$BEACON_HANDLER', 'beforeunload', '', '$SECONDARY_TEST_ROOT/add_instrumentation.html');" $WGET_OUTPUT) = 1 ]
check [ $(grep -c "pagespeed.addInstrumentationInit('/$BEACON_HANDLER', 'load', '', '$SECONDARY_TEST_ROOT/add_instrumentation.html');" $WGET_OUTPUT) = 1 ]
