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

# See bug 3103898 and
# http://carlosrivero.com/fix-apache
#---no-space-left-on-device-couldnt-create-accept-lock
#
# This script might help cleanup any leftover resources that prevent
# Apache from restarting.  The error message might look a little something
# like this:
#
# [Sat Oct 16 21:22:46 2010] [warn] pid file /usr/local/apache2/logs/httpd.pid
# overwritten -- Unclean shutdown of previous Apache run?
#
# [Sat Oct 16 21:22:46 2010] [emerg] (28)No space left on device: Couldn't
# create accept lock (/usr/local/apache2/logs/accept.lock.16025) (5)
#
# Usage:
#
#   devel/apache_cleanup $USER
#
# You may want to see the owners of the IPC blocks by running ipcs -s
# manually.  For example, you might need to run:
#
#   sudo devel/apache_cleanup www-data
# or
#   sudo devel/apache_cleanup root

apache_user=$1

for ipsemId in $(ipcs -s | grep $apache_user | cut -f 2 -d ' '); do
  echo ipcrm -s $ipsemId
  ipcrm -s $ipsemId || true
done
for ipsemId in $(ipcs -m | grep $apache_user | cut -f 2 -d ' '); do
  echo ipcrm -m $ipsemId
  ipcrm -m $ipsemId || true
done
