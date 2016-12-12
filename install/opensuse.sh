#!/bin/sh
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

set -x

SUFFIX=
if [ `uname -m` == x86_64 ]; then
    SUFFIX=64
fi

exec make \
    APACHE_CONTROL_PROGRAM=`readlink -f opensuse-initd-wrapper.sh` \
    APACHE_DOC_ROOT=/srv/www/htdocs \
    APACHE_LOG=/var/log/apache2/error_log \
    APACHE_MODULES=/usr/lib$SUFFIX/apache2 \
    APACHE_PIDFILE=/var/run/httpd2.pid \
    APACHE_PROGRAM=/usr/sbin/httpd2 \
    APACHE_ROOT=/etc/apache2 \
    APACHE_STOP_COMMAND=stop-graceful \
    APACHE_USER=wwwrun \
    BINDIR=/usr/local/bin \
    $*
