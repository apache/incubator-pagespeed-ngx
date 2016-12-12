#!/bin/bash
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

set -e
set -u

if [ $# -ne 1 ]; then
  echo "Usage: $(basename $0) <url_file>" >&2
  exit 1
fi

url_file=$1

if [ ! -e "$url_file" ]; then
  echo "Url file '$url_file' missing!" >&2
fi

# The size names must match install/build_psol.sh in mod_pagespeed
if [ "$(uname -m)" = x86_64 ]; then
  bit_size_name=x64
else
  bit_size_name=ia32
fi

sed -e 's/$BIT_SIZE_NAME\b/'$bit_size_name'/g' $url_file
