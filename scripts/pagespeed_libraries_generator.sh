#!/bin/bash
# Inspired by https://github.com/pagespeed/ngx_pagespeed/issues/532 
# run this file and direct output to a file , include said file and be sure to 
# enable canonicalize_javascript_libraries filter. 

curl -s https://modpagespeed.googlecode.com/svn/trunk/src/net/instaweb/genfiles/conf/pagespeed_libraries.conf \
     | grep ModPagespeedLibrary \
     | while read library size hash url ; do
  echo "  pagespeed Library $size $hash $url;"
done
