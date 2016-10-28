#!/bin/sh
#
# Builds the PSOL tarball for releases.  This is what ends up at
# https://dl.google.com/dl/page-speed/psol/${VERSION}.tar.gz
#
# Usage:
#
# 1. Build pagespeed_automatic.a in Debug and Release for 32 bit and 64 bit and
#    set up a psol directory like:
#
#    psol/lib/Debug/linux/x64/pagespeed_automatic.a
#    psol/lib/Debug/linux/ia32/pagespeed_automatic.a
#    psol/lib/Release/linux/x64/pagespeed_automatic.a
#    psol/lib/Release/linux/x64/pagespeed_js_minify
#    psol/lib/Release/linux/ia32/pagespeed_automatic.a
#    psol/lib/Release/linux/ia32/pagespeed_js_minify
#
# 2. Run install/build_psol_tarball.sh VERSION /path/to/psol
#
# This will:
#  * check that your psol/lib directory looks good
#  * prepare psol/include
#  * put its output in psol/
#  * create VERSION.tar.gz ready to be used to build ngx_pagespeed
#

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

WORKDIR=$(mktemp -d)
cd "$WORKDIR"

function die() {
  echo "build_psol_tarball.sh: $@"
  cd
  rm -rf "$WORKDIR"
  exit 1
}

function die_keep_workdir() {
  echo "build_psol_tarball.sh: $@"
  echo "(in $WORKDIR)"
  exit 1
}

if [ $# != 2 ]; then
  die "Usage: $0 VERSION /path/to/psol"
  exit 1
fi

VERSION="$1"
PSOL_PATH="$2"

if [ ! -d "$PSOL_PATH" ]; then
  die "$PSOL_PATH should be an existing directory"
fi

if [ $(basename "$PSOL_PATH") != "psol" ]; then
  die "$PSOL_PATH should end with /psol/"
fi

INCLUDE_DIR="$PSOL_PATH/include"
if [ -e "$INCLUDE_DIR" ]; then
  die "$INCLUDE_DIR already exists"
fi

missing_files=false
for expected in lib/Debug/linux/x64/pagespeed_automatic.a \
                lib/Debug/linux/ia32/pagespeed_automatic.a \
                lib/Release/linux/x64/pagespeed_automatic.a \
                lib/Release/linux/x64/pagespeed_js_minify \
                lib/Release/linux/ia32/pagespeed_automatic.a \
                lib/Release/linux/ia32/pagespeed_js_minify; do
  if [ ! -f "$PSOL_PATH/$expected" ]; then
    missing_files=true
    echo "missing $expected"
  fi
done
if $missing_files; then
  die "missing files under $PSOL_PATH"
fi

# psol binaries look good, now prepare the includes

mkdir mod_pagespeed
cd mod_pagespeed
git clone https://github.com/pagespeed/mod_pagespeed.git src
cd src
gclient config https://github.com/pagespeed/mod_pagespeed.git --unmanaged --name=$PWD
git checkout $VERSION
git submodule update --init --recursive
gclient sync --force --jobs=1

build_dir="$PWD"

if [[ "$VERSION" == 1.9.32.* ]]; then
  # On 1.9 (and earlier, but we don't build them anymore) automatic/ was in
  # a different place.
  automatic_dir=net/instaweb/automatic/
else
  automatic_dir=pagespeed/automatic/
fi

for buildtype in Release Debug; do
  make -j8 AR.host="$build_dir/build/wrappers/ar.sh" \
       AR.target="$build_dir/build/wrappers/ar.sh" \
       BUILDTYPE=$buildtype \
       mod_pagespeed_test pagespeed_automatic_test
  cd $automatic_dir
  make -j8 BUILDTYPE=$buildtype \
       -C $build_dir/$automatic_dir \
       AR.host="$build_dir/build/wrappers/ar.sh" \
       AR.target="$build_dir/build/wrappers/ar.sh" \
       all
  cd $build_dir
done

# Verify that it built properly.
cd $WORKDIR
for version_h in \
    mod_pagespeed/src/out/Debug/obj/gen/net/instaweb/public/version.h \
    mod_pagespeed/src/out/Release/obj/gen/net/instaweb/public/version.h; do
  if [ ! -f $version_h ]; then
    die_keep_workdir "Missing $version_h"
  fi
  if ! grep -q "^#define MOD_PAGESPEED_VERSION_STRING \"$VERSION\"$" \
            $version_h; then
    die_keep_workdir "Wrong version found in $version_h"
  fi
done

git clone https://github.com/pagespeed/ngx_pagespeed.git
cd ngx_pagespeed
git checkout trunk-tracking
scripts/prepare_psol.sh $WORKDIR/mod_pagespeed/src/
mv psol/include $INCLUDE_DIR

# Now index our .a files.
find $PSOL_PATH/lib -name '*.a' | xargs -n 1 ranlib

# Make the tarball.
cd $PSOL_PATH/..
tar -cvf $WORKDIR/$VERSION.tar psol/

echo "zipping the tarball"
gzip --best $WORKDIR/$VERSION.tar

mv $WORKDIR/$VERSION.tar.gz .

echo "Final PSOL tarball is $PWD/$VERSION.tar.gz"

rm -rf "$WORKDIR"
