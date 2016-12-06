#!/bin/sh
#
# Copyright 2014 Google Inc.
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
# Author: jmarantz@google.com (Joshua Marantz)
#
# Sets up a machine to run as a server for running mod_pagespeed tests.
# The tests include the fetching and proxying of a variety of content,
# including some of the /mod_pagespeed_example directory, plus some other
# content in /do_not_modify and /no_content.

if [ -z "$APACHE_DOC_ROOT" ]; then
  echo "This is not intended to be called directly.  It should be called"
  echo "from the makefile:"
  echo"   make setup_test_machine"
  exit 1
fi

set -e
set -u

for subdir in do_not_modify mod_pagespeed_example; do
  sudo rm -rf $APACHE_DOC_ROOT/$subdir
  tarfile=/tmp/subdir.$$.tmp
  (cd $GIT_SRC/install; tar czf $tarfile $subdir)
  (cd $APACHE_DOC_ROOT; sudo tar xf $tarfile)
  echo Updating $APACHE_DOC_ROOT/$subdir
  rm -f $tarfile
done

# We need to add the following configuration snippet into the global conf file,
# if not already present.
conf_file="$APACHE_CONF_DIR/pagespeed_test.conf"
if grep -q pagespeed_test.conf $APACHE_CONF_FILE; then
  echo $conf_file is already included in $APACHE_CONF_FILE
else
  tmp_conf=/tmp/pagespeed_test.conf.$$
  tmp_file=/tmp/conf_snippet.$$
  sed -e "s@APACHE_DOC_ROOT@$APACHE_DOC_ROOT@g" \
    < $GIT_SRC/install/test.conf.template \
    > $tmp_conf
  sudo cp $tmp_conf "$conf_file"
  rm -f $tmp_conf
  cp $APACHE_CONF_FILE $tmp_file
  include_line="Include \"$conf_file\""
  echo $include_line >> $tmp_file
  sudo cp $tmp_file $APACHE_CONF_FILE
  rm -f $tmp_file
  echo "Added \'$include_line\' to end of $APACHE_CONF_FILE"
fi

# TODO(jmarantz): the following lines are only valid for Ubuntu.  I am
# not yet sure how to automate this in an equivalent way on CentOS.
#
# This sequence is described in /usr/share/doc/apache2.2-common/README.Debian.gz
sudo a2ensite default-ssl
sudo a2enmod ssl
sudo a2enmod headers
sudo make-ssl-cert generate-default-snakeoil --force-overwrite

# TODO(jefftk): We don't restart the test servers often enough for this to be
# worth automating right now, but if it gets annoying then we should make it run
# automatically on server startup.
echo "In order for tests that check for servers on ports 8091 and 8092 to pass,"
echo "now and after every server restart please run:"
echo "  nohup scripts/serve_proxying_tests.sh > proxy_test_log.txt"
