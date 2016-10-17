# This test checks that the ClientDomainRewrite directive can turn on.
start_test ClientDomainRewrite on directive
HOST_NAME="http://client-domain-rewrite.example.com"
RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
  $HOST_NAME/mod_pagespeed_test/rewrite_domains.html)
MATCHES=$(echo "$RESPONSE_OUT" | grep -c pagespeed\.clientDomainRewriterInit)
check [ $MATCHES -eq 1 ]
