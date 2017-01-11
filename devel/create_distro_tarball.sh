#!/bin/bash
#
# Copyright 2011 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: morlovich@google.com (Maksim Orlovich)
#
# The usual mechanism used to develop mod_pagespeed and build binaries is based
# on merging all dependencies into a single source tree.  This script enables a
# standard  untar/configure/make flow that does not bundle widely available
# external libraries. It generates the tarball including the configure (or
# rather generate.sh) script.
#
# If --minimal is passed, it will cut out even more things. This was meant
# for packaging properly Debian, which has a particularly extensive package
# repository. At the moment this configuration requires further patching of
# the .gyp[i] files and doesn't work out of the box. The pruning was also done
# as of branch 33, so further tweaks might be required for this mode in
# 34 or newer.
#
# This is expected to be run from build_release_tarball.sh, on the branch you
# want a tarball for.

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

function usage {
  echo "create_distro_tarball_debian.sh [ --minimal ] tarball"
  exit 1
}

# This outputs a little wrapper around gyp that calls it with appropriate -D
# flag
function config {
  cat <<SCRIPT_END
#!/bin/sh
#
# This script uses gyp to generate Makefiles for mod_pagespeed built against
# the following system libraries:
#   apr, aprutil, apache httpd headers, icu, libjpeg_turbo, libpng, zlib.
#
# Besides the -D use_system_libs=1 below, you may need to set (via -D var=value)
# paths for some of these libraries via these variables:
#   system_include_path_httpd, system_include_path_apr,
#   system_include_path_aprutil.
#
# for example, you might run
# ./generate.sh -Dsystem_include_path_apr=/usr/include/apr-1 \\
#               -Dsystem_include_path_httpd=/usr/include/httpd
# to specify APR and Apache include directories.
#
# Also, BUILDTYPE=Release can be passed to make (the default is Debug).
echo "Generating src/Makefile"
src/build/gyp_chromium -D use_system_libs=1 \$*
SCRIPT_END
}

if [ $# -lt 1 ]; then
  usage
  exit
fi

MINIMAL=0
if [ "$1" == "--minimal" ]; then
  MINIMAL=1
  shift 1
fi

TARBALL="$1"
if [ -z "$TARBALL" ]; then
  usage
fi
touch "$TARBALL"
TARBALL="$(realpath $TARBALL)"
MPS_CHECKOUT="$PWD"

git submodule update --init --recursive

# Pick up our version info, and wrap src inside a modpagespeed-version dir.
source net/instaweb/public/VERSION
VER_STRING="$MAJOR.$MINOR.$BUILD.$PATCH"
TEMP_DIR="$(mktemp -d)"
WRAPPER_DIR="modpagespeed-$VER_STRING"
mkdir "$TEMP_DIR/$WRAPPER_DIR"
DIR="$WRAPPER_DIR/src"
ln -s "$MPS_CHECKOUT" "$TEMP_DIR/$DIR"

# Also create a little helper script that shows how to run gyp
config > "$TEMP_DIR/$WRAPPER_DIR/generate.sh"
chmod +x "$TEMP_DIR/$WRAPPER_DIR/generate.sh"

# Normally, the build system runs build/lastchange.sh to figure out what
# to put into the last portion of the version number. We are, however, going to
# be getting rid of the .git dirs, so that will not work (nor would it without
# network access). Luckily, we can provide the number via LASTCHANGE.in,
# so we just compute it now, and save it there.
./build/lastchange.sh "$MPS_CHECKOUT" > LASTCHANGE.in

# Things that depends on minimal or not.
if [ $MINIMAL -eq 0 ]; then
  GTEST=$DIR/testing
  GFLAGS=$DIR/third_party/gflags
  GIFLIB=$DIR/third_party/giflib
  ICU="$DIR/third_party/icu/icu.gyp \
      $DIR/third_party/icu/source/common/unicode/"
  JSONCPP=$DIR/third_party/jsoncpp
  LIBWEBP=$DIR/third_party/libwebp
  PROTOBUF=$DIR/third_party/protobuf
  RE2=$DIR/third_party/re2
else
  GTEST="$DIR/testing \
      --exclude $DIR/testing/gmock \
      --exclude $DIR/testing/gtest"
  GFLAGS=$DIR/third_party/gflags/gflags.gyp
  GIFLIB=$DIR/third_party/giflib/giflib.gyp
  ICU=$DIR/third_party/icu/icu.gyp
  JSONCPP=$DIR/third_party/jsoncpp/jsoncpp.gyp
  LIBWEBP="$DIR/third_party/libwebp/COPYING \
      $DIR/third_party/libwebp/examples/gif2webp_util.*"
  PROTOBUF="$DIR/third_party/protobuf/*.gyp \
      $DIR/third_party/protobuf/*.gypi"
  RE2=$DIR/third_party/re2/re2.gyp
fi

# It's tarball time!
# Note that this is highly-version specific, and requires tweaks for every
# release as its dependencies change.  Always run the version of this
# script that's on the branch you're making a tarball for.
cd "$TEMP_DIR"
tar cj --dereference --exclude='.git' --exclude='.svn' --exclude='.hg' -f $TARBALL \
    --exclude='*.mk' --exclude='*.pyc' \
    --exclude=$DIR/net/instaweb/genfiles/*/*.cc \
    $WRAPPER_DIR/generate.sh \
    $DIR/LASTCHANGE.in \
    $DIR/base \
    $DIR/build \
    --exclude $DIR/build/android/arm-linux-androideabi-gold \
    $DIR/install \
    $DIR/net/instaweb \
    $DIR/pagespeed \
    $DIR/strings \
    $GTEST \
    $DIR/third_party/apr/apr.gyp \
    $DIR/third_party/aprutil/aprutil.gyp \
    $DIR/third_party/aprutil/apr_memcache2.h \
    $DIR/third_party/aprutil/apr_memcache2.c \
    $DIR/third_party/httpd/httpd.gyp \
    $DIR/third_party/httpd24/httpd24.gyp \
    $DIR/third_party/base64 \
    $DIR/third_party/brotli \
    $DIR/third_party/chromium/src/base \
    --exclude src/third_party/chromium/src/base/third_party/xdg_mime \
    --exclude src/third_party/chromium/src/base/third_party/xdg_user_dirs \
    $DIR/third_party/chromium/src/build \
    --exclude $DIR/third_party/chromium/src/build/android \
    $DIR/third_party/chromium/src/googleurl \
    $DIR/third_party/chromium/src/net/tools \
    $DIR/third_party/closure/ \
    $DIR/third_party/closure_library/ \
    $DIR/third_party/css_parser \
    $DIR/third_party/domain_registry_provider \
    $GFLAGS \
    $GIFLIB \
    $DIR/third_party/google-sparsehash \
    $DIR/third_party/grpc \
    $DIR/third_party/hiredis \
    $ICU \
    $JSONCPP \
    $DIR/third_party/libjpeg_turbo/libjpeg_turbo.gyp \
    $DIR/third_party/libpng/libpng.gyp \
    $LIBWEBP \
    $DIR/third_party/modp_b64 \
    $DIR/third_party/optipng \
    $PROTOBUF  \
    $DIR/third_party/rdestl \
    $RE2 \
    $DIR/third_party/redis-crc \
    $DIR/third_party/serf \
    $DIR/third_party/zlib/zlib.gyp \
    $DIR/tools/gyp \
    $DIR/url

cd "$MPS_CHECKOUT"
rm -r "$TEMP_DIR"
