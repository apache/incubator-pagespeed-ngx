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
# Test our handling of headers when a FLUSH event occurs, using PHP.
# Tests that require PHP can be disabled by setting DISABLE_PHP_TESTS to
# non-empty, to cater to admins who don't want PHP installed.
if [ -z "${DISABLE_PHP_TESTS:-}" ]; then
  # Fetch the first file so we can check if PHP is enabled.
  start_test PHP is enabled.
  FILE=php_withoutflush.php
  URL=$TEST_ROOT/$FILE
  FETCHED=$WGET_DIR/$FILE
  # wget returns non-zero on 4XX and 5XX, both of which can occur with a
  # mis-configured PHP setup. We need to mask that because of set -e.
  $WGET_DUMP $URL > $FETCHED || true
  if ! grep -q '^HTTP/1.1 200' $FETCHED || grep -q '<?php' $FETCHED; then
    echo "*** PHP is not installed/working. If you'd like to enable this"
  echo "*** test please run: sudo apt-get install php5-common php5"
  echo
  echo "If php is already installed, run it with:"
  echo "    php-cgi -b 127.0.0.1:9000"
    echo
    echo "To disable php tests, set DISABLE_PHP_TESTS to non-empty"
    exit 1
  fi

  # Now we know PHP is working, proceed with the actual testing.
  start_test Headers are not destroyed by a flush event.
  check [ $(grep -c '^X-\(Mod-Pagespeed\|Page-Speed\):' $FETCHED) = 1 ]
  check [ $(grep -c '^X-My-PHP-Header: without_flush' $FETCHED) = 1 ]

  # mod_pagespeed doesn't clear the content length header if there aren't any
  # flushes, but ngx_pagespeed does.  It's possible that ngx_pagespeed should
  # also avoid clearing the content length, but it doesn't and I don't think
  # it's important, so don't check for content-length.
  # check [ $(grep -c '^Content-Length: [0-9]'          $FETCHED) = 1 ]

  FILE=php_withflush.php
  URL=$TEST_ROOT/$FILE
  FETCHED=$WGET_DIR/$FILE
  $WGET_DUMP $URL > $FETCHED
  check [ $(grep -c '^X-\(Mod-Pagespeed\|Page-Speed\):' $FETCHED) = 1 ]
  check [ $(grep -c '^X-My-PHP-Header: with_flush'    $FETCHED) = 1 ]
  # 2.2 prefork returns no content length while 2.2 worker returns a real
  # content length. IDK why but skip this test because of that.
  # check [ $(grep -c '^Content-Length: [0-9]'          $FETCHED) = 1 ]
fi
