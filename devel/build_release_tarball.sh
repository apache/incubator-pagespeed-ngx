#!/bin/bash
#
# Note: you might need to type your password a few times, once early, and once
# at the end.
#
# This should be run on a release branch to make sure we can make a tarball and
# at least build it on our workstations.  It will also copy the tarball into
# ~/release (where the binaries usually go).
#
# Like most of our dev tools this assumes Ubuntu 14 LTS.  If that isn't what you
# have, it's probably easiest to run this in a VM.
#
# Note that if this fails you may need to tweak the file list inside
# devel/create_distro_tarball.sh

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

function usage {
  echo "Usage: devel/build_release_tarball.sh <beta|stable>"
  exit 1
}

if [ $# -ne 1 ]; then
  usage
fi

if [ ! -d net/instaweb ]; then
  echo "This script must be run from the root of the mps checkout."
  exit 1
fi

source net/instaweb/public/VERSION
RELEASE="$MAJOR.$MINOR.$BUILD.$PATCH"
CHANNEL="$1"

deps="libpng12-dev libicu-dev libssl-dev libjpeg-dev realpath build-essential
      pkg-config gperf unzip libapr1-dev libaprutil1-dev apache2-dev"
if dpkg-query -Wf '${Status}\n' $deps 2>&1 | \
     grep -v "install ok installed"; then
  # Only run apt-get install if one of the deps is not already installed.
  # See: http://stackoverflow.com/questions/1298066
  sudo apt-get install $deps
fi

RELEASE_DIR="$HOME/release/$RELEASE"
mkdir -p "$RELEASE_DIR"
REVISION="$(build/lastchange.sh "$PWD" | sed 's/LASTCHANGE=//')"
TARBALL="$RELEASE_DIR/mod-pagespeed-$CHANNEL-$RELEASE-r$REVISION.tar.bz2"
devel/create_distro_tarball.sh "$TARBALL"

echo "Tarball should now be at $TARBALL"

# Try to build it
BUILD_DIR="$(mktemp -d)"
echo "Doing a test build inside $BUILD_DIR"
cd "$BUILD_DIR"

if openssl version | grep "^OpenSSL 1[.]0[.][01]\|^OpenSSL 0[.]"; then
  echo "Your openssl version is too old to build the tarball; we need 1.0.2+"
  echo "Building 1.0.2 from source..."
  OPENSSL_VERSION="1.0.2j"
  wget "https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz"
  tar -xzvf "openssl-${OPENSSL_VERSION}.tar.gz"
  cd openssl-"${OPENSSL_VERSION}"
  ./config --prefix="$BUILD_DIR" shared
  make
  make install
  export SSL_CERT_DIR=/etc/ssl/certs
  export PKG_CONFIG_PATH="$BUILD_DIR/lib/pkgconfig"
  export LD_LIBRARY_PATH="$BUILD_DIR/lib"
  cd "$BUILD_DIR"
fi

tar xjf "$TARBALL"
cd modpagespeed*
./generate.sh -Dsystem_include_path_apr=/usr/include/apr-1.0/ \
              -Dsystem_include_path_httpd=/usr/include/apache2
cd src
make -j6
out/Debug/mod_pagespeed_test
# These tests fail because they are golded against a specific version of
# compression libraries.
# TODO(sligocki): Could we change the tests to be less fragile or test in a
# different way in this case?
BROKEN_TESTS=\
ImageConverterTest.OptimizePngOrConvertToJpeg:\
ImageConverterTest.ConvertOpaqueGifToJpeg:\
JpegOptimizerTest.ValidJpegsLossy:\
JpegOptimizerTest.ValidJpegLossyAndColorSampling:\
JpegOptimizerTest.ValidJpegsProgressiveAndLossy
out/Debug/pagespeed_automatic_test --gtest_filter=-$BROKEN_TESTS

echo "Cleaning up"
rm -rf "$BUILD_DIR"

