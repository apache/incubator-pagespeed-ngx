#!/bin/bash
#
# Copyright 2010 Google Inc.
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
#
# Script to install a debuggable mod_pagespeed.so into the Apache
# distribution, usually in ~/apache2.

APACHE_DEBUG_ROOT=$1
APACHE_DEBUG_PORT=$2
SRC_TREE=$3

mkdir -p $APACHE_DEBUG_ROOT/pagespeed/cache
chmod a+rwx $APACHE_DEBUG_ROOT/pagespeed/cache

# Check to see of mod_pagespeed is already loaded into httpd.conf
conf_file=$APACHE_DEBUG_ROOT/conf/httpd.conf
if [ -e $conf_file ]; then
  if grep -q "^Listen $APACHE_DEBUG_PORT\$" $conf_file; then
    echo $conf_file is set up to listen on the port $APACHE_DEBUG_PORT.
  else
    echo $conf_file is not set up to listen on port $APACHE_DEBUG_PORT
    echo please remedy
    exit 1
  fi

  if grep -q "LoadModule pagespeed_module" $conf_file; then
    echo mod_pagespeed is already loaded in config file $conf_file
  else
    echo adding mod_pagespeed into apache config file $conf_file
    cat $SRC_TREE/install/common/pagespeed.load.template | \
      sed s#@@APACHE_MODULEDIR@@#$APACHE_DEBUG_ROOT/modules# | \
      sed s#@@COMMENT_OUT_DEFLATE@@## >> $conf_file
    echo Include $APACHE_DEBUG_ROOT/conf/pagespeed.conf >> $conf_file
  fi

  # Now hack the file to also load mod_h2.
  MOD_H2=$APACHE_DEBUG_ROOT/modules/mod_http2.so
  if [ -f $MOD_H2 ]; then
    if grep -q "LoadModule http2_module" $conf_file; then
      echo http2_module is already loaded in config file $conf_file
    else
      echo adding http2_module into apache config file $conf_file
      cat $conf_file | sed -e '/pagespeed.conf/i\
\
# Load mod_http2 to test mod_pagespeed integration. This is done before\
# pagespeed.conf so it can detect it.\
LoadModule http2_module '$MOD_H2'\
Protocols h2 http/1.1 \
Protocols h2c http/1.1\
' > $conf_file.sp
      mv $conf_file.sp $conf_file
    fi
  else
    echo "No mod_http2 in $APACHE_DEBUG_ROOT/modules, so not loading"
  fi

  # pagespeed_libraries.conf was added later, so check for it separately.
  libraries_conf_file="$APACHE_DEBUG_ROOT/conf/pagespeed_libraries.conf"
  if grep -q "Include $libraries_conf_file" $conf_file; then
    echo pagespeed_libraries.conf is already loaded by $conf_file
  else
    echo adding pagespeed_libraries.conf include to $conf_file
    cp -f $SRC_TREE/net/instaweb/genfiles/conf/pagespeed_libraries.conf \
          $libraries_conf_file
    echo Include $libraries_conf_file >> $conf_file
  fi
else
  echo "$conf_file does not exist.  Consider updating devel/Makefile and/or"
  echo "devel/apache_install.sh"
  exit 1
fi

exit 0
