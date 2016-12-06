#!/bin/bash
# Install mod_pagespeed testing version of Apache for use in checkin test.
# This also installs the dependencies for mod_h2 if you're building 2.4
#
# Usage
#   install/build_development_apache.sh 2.2|2.4 worker|event|prefork|prefork-debug

set -e
set -u

this_dir="$(dirname "${BASH_SOURCE[0]}")"
src="$this_dir/.."
third_party="$(readlink -m "$src/third_party")"

"$src/install/install_required_packages.sh" --additional_dev_packages

if [ $# -ne 2 ]; then
  echo Usage: $0 '2.2|2.4 worker|event|prefork|prefork-debug'
  exit 1
fi

if [ x$1 == x2.4 ]; then
  HTTPD_DIR="httpd24"
elif [ x$1 == x2.2 ]; then
  HTTPD_DIR="httpd"
else
  echo Usage: $0 '2.2|2.4 worker|event|prefork|prefork-debug'
  exit 1
fi

TARGET=${TARGET:-$HOME/apache2}

# To facilitate switching between multiple Apache builds, a developer may make
# $TARGET be a sym-link.  To avoid installing over the sym-link, it should be
# deleted before you compile a new system.
if [ -L $TARGET ]; then
  echo Please remove symlink $TARGET before building over it.
  exit 1
fi

MPM=$2
if [ x$MPM == xprefork-debug ]; then
  CONFIGURE_ARGS=--enable-pool-debug
  MPM=prefork
else
  CONFIGURE_ARGS=--enable-debugger-mode
fi

# There does not appear to be a good reason to make different source directories
# for different MPMs -- the sources do not change so we can debug against any of
# them.

cd "$third_party/$HTTPD_DIR/src"

if [ -z "$(ls)" ]; then
  echo "It looks like you haven't loaded submodules.  Could you run:"
  echo "  git submodule update --init --recursive"
  echo "and then try again?"
  exit 1
fi

cd "$third_party/apr/src"
./buildconf --prefix=$TARGET
./configure --prefix=$TARGET
make
make install

cd "$third_party/aprutil/src"
./buildconf --with-apr="$third_party/apr/src" --prefix=$TARGET
./configure --with-apr="$third_party/apr/src" --prefix=$TARGET
make
make install

if [ "$HTTPD_DIR" = "httpd24" ]; then
  # nghttp2 depends on Apache 2.4+, so only build it for 2.4.
  cd "$third_party/nghttp2"
  echo "Configuring nghttp2"
  ./configure --prefix=$TARGET
  echo "Building nghttp2"
  make
  make install
  CONFIGURE_ARGS="$CONFIGURE_ARGS --enable-http2 --with-nghttp2=$TARGET"
fi

# Now actually configure, build, and install Apache.
cd "$third_party/$HTTPD_DIR/src"
./buildconf --prefix=$TARGET \
  --with-apr="$third_party/apr/src" --with-apr-util="$third_party/aprutil/src"
./configure --enable-proxy --enable-proxy-http --enable-rewrite --enable-so \
  --enable-deflate --enable-modules=all --with-mpm=$MPM \
  --enable-ssl --with-port=8080 --with-sslport=8443 --prefix=$TARGET \
  --with-apr="$third_party/apr/src" --with-apr-util="$third_party/aprutil/src" \
  $CONFIGURE_ARGS
make -j4

echo Apache build done

echo Installing Apache in $TARGET ...
make install

echo Building mod_fcgid
cd "$third_party/mod_fcgid"
APXS=$TARGET/bin/apxs ./configure.apxs
make -j4
echo mod_fcgid build done

$TARGET/build-1/libtool --silent --mode=install cp modules/fcgid/mod_fcgid.la \
    $TARGET/modules/
mv $TARGET/modules/mod_fcgid.so $TARGET/modules/mod_fcgid-src_build.so
exit 0
