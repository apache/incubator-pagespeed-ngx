#!/bin/sh
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

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

function usage {
  echo -n "create_distro_tarball_debian.sh [ --minimal ] [ --checkout_dir dir ] tarball"
  echo " [ --git_tag tag | source_tree ]"
  echo "examples of git_tag would be 'master', '33', '1.11.33.4' or 04e3237"
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

if [ $# -lt 2 ]; then
  usage
  exit
fi

CHECKOUT_DIR=
if [ "$1" = "--checkout_dir" ]; then
  if [ -z $2 ]; then
    usage
  fi
  CHECKOUT_DIR=$2
  shift 2
else
  CHECKOUT_DIR=$(mktemp -d)
  echo $CHECKOUT_DIR
fi

MINIMAL=0
if [ "$1" == "--minimal" ]; then
  MINIMAL=1
  shift 1
fi

TARBALL=$1
if [ -z $TARBALL ]; then
  usage
fi
touch $TARBALL
TARBALL=$(realpath $TARBALL)

if [ "$2" == --git_tag ]; then
  if [ -z $3 ]; then
    usage
  else
    GIT_TAG=$3
    if [ -z $CHECKOUT_DIR ]; then
      CHECKOUT_DIR=$(mktemp -d)
    fi
    SRC_DIR=$CHECKOUT_DIR
    cd $CHECKOUT_DIR
    git clone https://github.com/pagespeed/mod_pagespeed.git src\
      --branch "$GIT_TAG" --recursive
    cd src
  fi
else
  SRC_DIR=$2
fi

cd $SRC_DIR

# Pick up our version info, and wrap src inside a modpagespeed-version dir,
# so unpackers don't get a src/ surprise
source src/net/instaweb/public/VERSION
VER_STRING=$MAJOR.$MINOR.$BUILD.$PATCH
DIR=modpagespeed-$VER_STRING
rm -rf $DIR
mkdir $DIR
echo ln -sf $SRC_DIR/src $DIR/src/
ln -sf $SRC_DIR/src $DIR/src

# Also create a little helper script that shows how to run gyp
config > $DIR/generate.sh
chmod +x $DIR/generate.sh

# Normally, the build system runs build/lastchange.sh to figure out what
# to put into the last portion of the version number. We are, however, going to
# be getting rid of the .git dirs, so that will not work (nor would it without
# network access). Luckily, we can provide the number via LASTCHANGE.in,
# so we just compute it now, and save it there.
cd src/
./build/lastchange.sh $PWD > LASTCHANGE.in
cd ..

# Things that depends on minimal or not.
if [ $MINIMAL -eq 0 ]; then
  GTEST=$DIR/src/testing
  GFLAGS=$DIR/src/third_party/gflags
  GIFLIB=$DIR/src/third_party/giflib
  ICU="$DIR/src/third_party/icu/icu.gyp \
      $DIR/src/third_party/icu/source/common/unicode/"
  JSONCPP=$DIR/src/third_party/jsoncpp
  LIBWEBP=$DIR/src/third_party/libwebp
  PROTOBUF=$DIR/src/third_party/protobuf
  RE2=$DIR/src/third_party/re2
else
  GTEST="$DIR/src/testing \
      --exclude $DIR/src/testing/gmock \
      --exclude $DIR/src/testing/gtest"
  GFLAGS=$DIR/src/third_party/gflags/gflags.gyp
  GIFLIB=$DIR/src/third_party/giflib/giflib.gyp
  ICU=$DIR/src/third_party/icu/icu.gyp
  JSONCPP=$DIR/src/third_party/jsoncpp/jsoncpp.gyp
  LIBWEBP="$DIR/src/third_party/libwebp/COPYING \
      $DIR/src/third_party/libwebp/examples/gif2webp_util.*"
  PROTOBUF="$DIR/src/third_party/protobuf/*.gyp \
      $DIR/src/third_party/protobuf/*.gypi"
  RE2=$DIR/src/third_party/re2/re2.gyp
fi

# It's tarball time!
# Note that this is highly-version specific, and requires tweaks for every
# release as its dependencies change.  Always run the version of this
# script that's on the branch you're making a tarball for.
tar cj --dereference --exclude='.git' --exclude='.svn' --exclude='.hg' -f $TARBALL \
    --exclude='*.mk' --exclude='*.pyc' \
    --exclude=$DIR/src/net/instaweb/genfiles/*/*.cc \
    $DIR/generate.sh \
    $DIR/src/LASTCHANGE.in \
    $DIR/src/base \
    $DIR/src/build \
    --exclude $DIR/src/build/android/arm-linux-androideabi-gold \
    $DIR/src/install \
    $DIR/src/net/instaweb \
    $DIR/src/pagespeed \
    $DIR/src/strings \
    $GTEST \
    $DIR/src/third_party/apr/apr.gyp \
    $DIR/src/third_party/aprutil/aprutil.gyp \
    $DIR/src/third_party/aprutil/apr_memcache2.h \
    $DIR/src/third_party/aprutil/apr_memcache2.c \
    $DIR/src/third_party/httpd/httpd.gyp \
    $DIR/src/third_party/httpd24/httpd24.gyp \
    $DIR/src/third_party/base64 \
    $DIR/src/third_party/brotli \
    $DIR/src/third_party/chromium/src/base \
    --exclude src/third_party/chromium/src/base/third_party/xdg_mime \
    --exclude src/third_party/chromium/src/base/third_party/xdg_user_dirs \
    $DIR/src/third_party/chromium/src/build \
    --exclude $DIR/src/third_party/chromium/src/build/android \
    $DIR/src/third_party/chromium/src/googleurl \
    $DIR/src/third_party/chromium/src/net/tools \
    $DIR/src/third_party/closure/ \
    $DIR/src/third_party/closure_library/ \
    $DIR/src/third_party/css_parser \
    $DIR/src/third_party/domain_registry_provider \
    $GFLAGS \
    $GIFLIB \
    $DIR/src/third_party/google-sparsehash \
    $DIR/src/third_party/grpc \
    $DIR/src/third_party/hiredis \
    $ICU \
    $JSONCPP \
    $DIR/src/third_party/libjpeg_turbo/libjpeg_turbo.gyp \
    $DIR/src/third_party/libpng/libpng.gyp \
    $LIBWEBP \
    $DIR/src/third_party/modp_b64 \
    $DIR/src/third_party/optipng \
    $PROTOBUF  \
    $DIR/src/third_party/rdestl \
    $RE2 \
    $DIR/src/third_party/redis-crc \
    $DIR/src/third_party/serf \
    $DIR/src/third_party/zlib/zlib.gyp \
    $DIR/src/tools/gyp \
    $DIR/src/url

if [ x != x$CHECKOUT_DIR ]; then
  echo "You may want to rm -rf $CHECKOUT_DIR to clean up"
fi
