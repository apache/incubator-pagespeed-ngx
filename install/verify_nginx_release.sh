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
#
# Checks that an nginx release builds and passes tests.  This ensures that the
# PSOL tarball is good, and that it's compatible with the nginx code we intend
# to release.
#
# Usage:
#
#   verify_nginx_release.sh [version] [binary tarball]
#   verify_nginx_release.sh 1.10.33.6 /path/to/1.10.33.6.tar.gz
#
# To get the binary tarball, run build_psol_tarball.sh

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

if [ $# != 2 ]; then
  echo "Usage: $0 version /path/to/psol-binary-tarball"
  exit 1
fi

VERSION="$1"
TARBALL="$2"

# Absoluteify $TARBALL if it does not start with /
if [ -n "$TARBALL" -a "${TARBALL#/}" = "$TARBALL" ]; then
  TARBALL="$PWD/$TARBALL"
fi

if [ ! -f "$TARBALL" ]; then
  echo "$TARBALL should be a file"
  exit 1
fi

die() {
  echo "verify_nginx_release.sh: $@"
  cd
  rm -rf "$WORKDIR"
  exit 1
}

WORKDIR=$(mktemp -d)
cd "$WORKDIR"

mkdir mod_pagespeed
cd mod_pagespeed
git clone https://github.com/pagespeed/mod_pagespeed.git src/
cd src/
git checkout $VERSION

cd $WORKDIR
git clone https://github.com/pagespeed/ngx_pagespeed.git
cd ngx_pagespeed
git checkout release-$VERSION-beta

# We now include the url for the PSOL binary that goes with a release in a
# separate file.
if [ ! -e PSOL_BINARY_URL ]; then
  echo "$PWD/PSOL_BINARY_URL is missing"
  exit 1
else
  predicted_psol_binary_url="https://dl.google.com/dl/page-speed/psol/"
  predicted_psol_binary_url+="$VERSION.tar.gz"
  psol_binary_url=$(cat PSOL_BINARY_URL)
  if [ "$predicted_psol_binary_url" != "$psol_binary_url" ]; then
    echo "PSOL_BINARY_URL is wrong; did you forget to update it?  Got:"
    echo "$psol_binary_url"
    exit 1
  fi
fi

tar -xzf "$TARBALL"

cd $WORKDIR
git clone https://github.com/FRiCKLE/ngx_cache_purge.git

# If ldconfig is not found, add /sbin to the path. ldconfig is required
# for openresty with luajit.
if ! type -t ldconfig >/dev/null && [ -e /sbin/ldconfig ]; then
  PATH=$PATH:/sbin
fi

wget https://openresty.org/download/openresty-1.9.7.3.tar.gz
tar xzvf openresty-*.tar.gz
cd openresty-*/
./configure --with-luajit
make

cd $WORKDIR
wget http://nginx.org/download/nginx-1.9.12.tar.gz

for is_debug in debug release; do
  cd $WORKDIR
  if [ -d nginx ]; then
    rm -rf nginx/
  fi
  tar -xzf nginx-1.9.12.tar.gz
  mv nginx-1.9.12 nginx
  cd nginx/

  nginx_root="$WORKDIR/$is_debug/"

  extra_args=""
  if [ "$is_debug" = "debug" ]; then
    extra_args+=" --with-debug"
  fi

  if [ -x /usr/lib/gcc-mozilla/bin/gcc ]; then
    PATH=/usr/lib/gcc-mozilla/bin:$PATH
    extra_args+=" --with-cc=/usr/lib/gcc-mozilla/bin/gcc --with-ld-opt=-static-libstdc++"
  fi

  ./configure \
    --prefix="$nginx_root" \
    --add-module="$WORKDIR/ngx_pagespeed" \
    --add-module="$WORKDIR/ngx_cache_purge" \
    --add-module="$WORKDIR/openresty-*/build/ngx_devel_kit-*/" \
    --add-module="$WORKDIR/openresty-*/build/set-misc-nginx-*/" \
    --add-module="$WORKDIR/openresty-*/build/headers-more-nginx-module-*/" \
    --with-ipv6 \
    --with-http_v2_module \
    $extra_args

  make install

  cd "$WORKDIR"
  USE_VALGRIND=false \
    TEST_NATIVE_FETCHER=false \
    TEST_SERF_FETCHER=true \
    ngx_pagespeed/test/run_tests.sh 8060 8061 \
    $WORKDIR/mod_pagespeed \
    $nginx_root/sbin/nginx \
    modpagespeed.com

  cd
done

rm -rf "$WORKDIR"

echo "builds and tests completed successfully for both debug and release"
