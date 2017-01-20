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

# Usage: scripts/rebuild.sh
#
# After building with "scripts/build_ngx_pagespeed.sh --devel", if you make
# changes to ngx_pagespeed you'll need to rebuild it.  The underlying commands
# aren't complicated, but it's faster to work if it's automated.

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

this_dir="$(dirname $0)"
cd "$this_dir/.."
nps_dir="$PWD"

cd "$nps_dir/testing-dependencies/mod_pagespeed/devel"
make apache_debug_psol

cd "$nps_dir/testing-dependencies/nginx/"
make
make install
