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
start_test HTML add_instrumentation CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.html\
?PageSpeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
# In some servers PageSpeed runs before response headers are finalized, which
# means it has to  assume the page is xhtml because the 'Content-Type' header
# might just not have been set yet.  In others it runs after, and so it can
# trust what it sees in the headers.  See RewriteDriver::MimeTypeXhtmlStatus().
if $HEADERS_FINALIZED; then
  check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 0 ]
else
  check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]
fi

start_test XHTML add_instrumentation also lacks '&amp;' and contains CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.xhtml\
?PageSpeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]
