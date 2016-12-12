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

# This script prepares ngx_pagespeed_example.tar.gz for ngxpagespeed.com
# Usage:
#   cd install/
#   ./prepare_ngx_pagespeed_examples.sh

set -u
set -e

if [ ! -d mod_pagespeed_example/ ]; then
  echo "This script expects to be run in the directory that contains"
  echo "mod_pagespeed_example/"
  exit 1
fi

tgz_out=$PWD/ngx_pagespeed_example.tar.gz
workdir=$(mktemp -d)
trap "rm -r $workdir" EXIT

nps_example=$workdir/ngx_pagespeed_example/

echo "Building ngx_pagespeed_example.tar.gz ..."

cp -r mod_pagespeed_example/ $nps_example

find $nps_example -name "*.html" | while read fname; do
  sed -i s~mod_pagespeed~ngx_pagespeed~g "$fname"
  sed -i s~mod-pagespeed~ngx-pagespeed~g "$fname"
  sed -i s~Apache~Nginx~g "$fname"
done

# For backwards compatibility, create a symlink so that someone can visit either
# ngxpagespeed.com or ngxpagespeed.com/ngx_pagespeed_example/ without issue.
ln -s . $nps_example/ngx_pagespeed_example

tar -czf $tgz_out -C $workdir ngx_pagespeed_example/

echo " ... built"
echo "Now scp it up to ngxpagespeed.com, and run:"
echo "  tar -xzf ngx_pagespeed_example.tar.gz"
echo "  sudo rm -rf /usr/local/nginx/html"
echo "  sudo mv ngx_pagespeed_example /usr/local/nginx/html"
echo "  sudo chmod ugo+rwX -R /usr/local/nginx/html"



