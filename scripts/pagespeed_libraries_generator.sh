#!/bin/bash
#
# Converts pagespeed_libraries.conf from Apache-format to Nginx-format,
# supporting the canonicalize_javascript_libraries filter.
# Inspired by https://github.com/pagespeed/ngx_pagespeed/issues/532
#
# Usage:
#   scripts/pagespeed_libraries_generator.sh > pagespeed_libraries.conf
#
# Then have nginx include that configuration file and enable the filter
# canonicalize_javascript_libraries.
#
# Author: vid@zippykid.com (Vid Luther)
#         jefftk@google.com (Jeff Kaufman)

URL="https://modpagespeed.googlecode.com/svn/trunk/src/"
URL+="net/instaweb/genfiles/conf/pagespeed_libraries.conf"
curl -s "$URL" \
    | grep ModPagespeedLibrary \
    | while read library size hash url ; do
  echo "  pagespeed Library $size $hash $url;"
done
