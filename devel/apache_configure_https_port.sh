#!/bin/bash
#
# Copyright 2011 Google Inc.
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
# Script to enable SSL on the given port for Apache, usually in ~/apache2.
#
# usage: apache_configure_https_port.sh apache-root-directory https-port

APACHE_ROOT=$1
HTTPS_PORT=$2

# If either argument is missing, do nothing (assume that https is disabled).
[ -z "$APACHE_ROOT" ] && exit 0
[ -z "$HTTPS_PORT" ] && exit 0

# Change the port only if we can find the file where we expect it.
conf_file=$APACHE_ROOT/conf/extra/httpd-ssl.conf
if [ -e $conf_file ]; then
  sed -e '/^[ 	]*Listen /s/^.*$/Listen '"$HTTPS_PORT"'/' \
      -e '/<VirtualHost /s/.*:[0-9]*/<VirtualHost localhost:'"$HTTPS_PORT"'/' \
      -e '/^[ 	]*ServerName /s/^.*$/ServerName '"$(hostname):$HTTPS_PORT"'/' \
      ${conf_file} > ${conf_file}.$$
  if mv -f ${conf_file}.$$ ${conf_file}; then
    echo HTTPS was enabled on port $HTTPS_PORT in $conf_file
  else
    rm -f ${conf_file}.$$
    echo FAILED: mv ${conf_file}.$$ ${conf_file}
    echo Cannot enable HTTPS on port $HTTPS_PORT in $conf_file
  fi
else
  echo $conf_file does not exist.
  echo Consider updating devel/Makefile and/or devel/$(basename $0)
  exit 1
fi

exit 0
