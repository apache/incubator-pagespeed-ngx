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
# This test checks that the DomainRewriteHyperlinks directive
# can turn off.  See mod_pagespeed_test/rewrite_domains.html: it has
# one <img> URL, one <form> URL, and one <a> url, all referencing
# src.example.com.  Only the <img> url should be rewritten.
start_test RewriteHyperlinks off directive
HOST_NAME="http://domain-hyperlinks-off.example.com"
RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    $HOST_NAME/mod_pagespeed_test/rewrite_domains.html)
MATCHES=$(echo "$RESPONSE_OUT" | fgrep -c http://dst.example.com)
check [ $MATCHES -eq 1 ]

# This test checks that the DomainRewriteHyperlinks directive
# can turn on.  See mod_pagespeed_test/rewrite_domains.html: it has
# one <img> URL, one <form> URL, and one <a> url, all referencing
# src.example.com.  They should all be rewritten to dst.example.com.
start_test RewriteHyperlinks on directive
HOST_NAME="http://domain-hyperlinks-on.example.com"
RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    $HOST_NAME/mod_pagespeed_test/rewrite_domains.html)
MATCHES=$(echo "$RESPONSE_OUT" | fgrep -c http://dst.example.com)
check [ $MATCHES -eq 4 ]
