#!/bin/bash
#
# Copyright 2012 Google Inc.
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
# This script is intended to be run from devel/mps_load_test.sh, although it can
# be run directly as well.
#
# Usage:  devel/mps_generate_load.sh \
#             [--ipro_preserve] [--ssl] [--user_agent user_agent_string]

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

devel_directory="$(dirname $0)"

corpus_suffix=
IPRO_PRESERVE=0
if [[ $# -ge 1 && "$1" = "--ipro_preserve" ]]; then
  shift
  corpus_suffix=.ipro_preserve
  IPRO_PRESERVE=1
fi

extra_flags=
if [[ $# -ge 1 && "$1" = "--ssl" ]]; then
  shift
  extra_flags=$1
fi

user_agent=
if [[ $# -ge 1 && "$1" = "--user_agent" ]]; then
  user_agent=$2
  shift 2
fi

corpus_file=/tmp/corpus_all_urls.txt.$USER$corpus_suffix

# Grab the file from the server host if needed.
if [ ! -e $corpus_file ]; then
  work_file=$(mktemp)
  src="$HOME/pagespeed-loadtest-corpus/corpus_all_urls.txt"
  cp $src $work_file
  if [ $IPRO_PRESERVE = 1 ]; then
    cat $work_file | fgrep -v .pagespeed. > $corpus_file
    rm $work_file
  else
    mv $work_file $corpus_file
  fi
fi

PROXY_HOST=127.0.0.1 FLAGS=$extra_flags USER_AGENT=$user_agent PAR=50 RUNS=3 \
    $devel_directory/trace_stress_test.sh $corpus_file
