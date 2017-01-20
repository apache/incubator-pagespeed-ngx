#!/bin/bash
#
# Copyright 2017 Google Inc.
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
# This script collects slurps and URLs (post-optimization, if possible), of
# some websites with the help of phantomjs.

function usage {
  echo "Usage: loadtest_collect/loadtest_collect_corpus.sh pages.txt out.tar.bz2"
  echo "Where pages.txt has a URL (including http://) per line"
}

set -u  # exit the script if any variable is uninitialized
set -e

if [ $# -ne 2 ]; then
  usage
  exit 1
fi

if [ -d devel ]; then
  cd devel
fi
if [ ! -d loadtest_collect ]; then
  echo Run this script from the top or devel/ directories
  exit 1
fi

if [ ! $(which phantomjs) ]; then
  echo "phantomjs not found, trying to install it with apt-get"
  sudo apt-get install phantomjs
fi

SLURP_TOP_DIR=$(mktemp -d)
SLURP_DIR=$SLURP_TOP_DIR/slurp
mkdir $SLURP_DIR
LOG_PATH=$SLURP_TOP_DIR/log.txt
URLS_PATH=$SLURP_TOP_DIR/corpus_all_urls.txt

make clean_slate_for_tests
make apache_debug_stop

sed -e "s^#HOME^$HOME^" -e "s^#SLURP_DIR^$SLURP_DIR^" \
  -e "s^#LOG_PATH^$LOG_PATH^" \
  < loadtest_collect/loadtest_collect.conf > ~/apache2/conf/pagespeed.conf
make -j8 apache_debug_restart

for site in $(cat $1); do
  echo $site
  phantomjs --proxy=127.0.0.1:8080 loadtest_collect/script.js $site
done
cat $LOG_PATH | grep ^GET | cut -d ' ' -f 2 > $URLS_PATH
cd $SLURP_TOP_DIR
tar cvjf $2 .

