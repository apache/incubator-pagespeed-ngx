#!/bin/bash
#
# Builds a mod_pagespeed distribution.
#
# Usage:
#   1. [optional] Log into buildbot
#   2. ./build_release_platform.sh [-clean] $RELEASE $CHANNEL [patch_file] [tag]
#
# Where $RELEASE is either the 4 segment version number, e.g 0.10.21.2, or the
# word "trunk" and $CHANNEL is either "beta" or "stable".
#
# The optional "patch_file" will be applied after runing 'gclient sync'.
# Also note that if you re-run the same build command multiple times,
# pre-applied patches will remain.
#
# If tag is specified it will be used to pick what gets checked out, instead of
# $RELEASE; this is useful if applying an unreleased patch that changes the
# version number for a security release.
#
# Updating:
#   If you make any changes to this script, please scp it up to the builbotd:
#      scp build_release_platform.sh [IP-OF-CENTOS-BUILDBOT]:
#      scp build_release_platform.sh [IP-OF-UBUNTU-BUILDBOT]:
#
#   The builbots have hardlinks between /home/builbot/build_release_platform.sh
#   and /var/chroot/[chroot-name]/home/buildbot/build_release_platform.sh, which
#   means that when you update the copy in ~/build_release_platform.sh you're
#   automatically updating the one in the chroot as well.


set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized
export VIRTUALBOX_TEST="VIRTUALBOX_TEST"  # to skip some tests that fail in VM

# Cleanup /var/html if desired before running.
if [ "$1" = "-clean" ]; then
  clean=1
  shift
else
  clean=0
fi

RELEASE=$1
CHANNEL=$2
TAG=$RELEASE

do_patch="0"

DOC_ROOT=/var/www
[ -d /var/www/html ] && DOC_ROOT=/var/www/html

if [ -d $DOC_ROOT/do_not_modify -a \
     ! -L $DOC_ROOT/do_not_modify ] || \
   [ -d $DOC_ROOT/mod_pagespeed_example -a \
     ! -L $DOC_ROOT/mod_pagespeed_example ] || \
   [ -d $DOC_ROOT/mod_pagespeed_test -a \
     ! -L $DOC_ROOT/mod_pagespeed_test ]; then
  if [ $clean -eq 0 ]; then
    echo Stale directories in $DOC_ROOT exist.  Clean them
    echo yourself or specify -clean as first arg to let the script do that
    echo for you.
    exit 1
  fi
  set -x
  sudo rm -rf $DOC_ROOT/do_not_modify
  sudo rm -rf $DOC_ROOT/mod_pagespeed_example
  sudo rm -rf $DOC_ROOT/mod_pagespeed_test
else
  set -x
fi

# Apply optional patch-file to apply before building.  To create a patch from a
# commit, do:
#   git show [commit id] > patchfile
if [ $# -ge 3 ]; then
  patch_file=$(readlink -f $3)
  do_patch="1"
fi

if [ $# -eq 4 ]; then
  TAG=$4
fi

if [ -d ~/bin/depot_tools ]; then
  cd ~/bin/depot_tools
  git pull
else
  mkdir -p ~/bin
  cd ~/bin
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
fi

PATH=~/bin/depot_tools:$PATH

# Are we on CentOS or Ubuntu?
# TODO(jmarantz): make this work on RedHat.
#
# Note that the 'force' directives on the install commands are only
# used for this RPM/DEB build/install/test script, and are not
# exported as a recommended flow for users.
#
# If the force directives are not included, then the installation will
# fail if run a second time.
if grep -q CentOS /etc/issue; then
  # We redirect stderr to stdout because on Centos5 "httpd -M" writes to stderr
  # for some reason.
  /usr/sbin/httpd -M 2>&1 | grep -q php5_module || \
    sudo yum install php php-mbstring

  EXT=rpm
  INSTALL="rpm --install"
  RESTART="./centos.sh apache_debug_restart"
  TEST="./centos.sh enable_ports_and_file_access apache_vm_system_tests"

  # To build on Centos 5/6 we need gcc 4.8 from scientific linux.  We can't
  # export CC and CXX because some steps still use a literal "g++".  But #$%^
  # devtoolset includes its own sudo, and we don't want that because it doesn't
  # support -E, so rename it if it exists before updating PATH.
  DEVTOOLSET_BIN=/opt/rh/devtoolset-2/root/usr/bin/
  if [ -e "$DEVTOOLSET_BIN/sudo" ]; then
    sudo mv "$DEVTOOLSET_BIN/sudo" "$DEVTOOLSET_BIN/sudo.ignored"
  fi
  export PATH="$DEVTOOLSET_BIN:$PATH"

  echo We appear to be running on CentOS.  Building rpm...
else
  echo Making sure PHP is installed ...
  apache2ctl -M | grep -q php5_module || \
    sudo apt-get install libapache2-mod-php5

  EXT=deb
  INSTALL="dpkg --install"
  RESTART="./ubuntu.sh apache_debug_restart"
  TEST="./ubuntu.sh apache_vm_system_tests"

  if [ -d /usr/lib/gcc-mozilla/bin ]; then
    export PATH=/usr/lib/gcc-mozilla/bin:$PATH
  fi

  echo We appear to NOT be running on CentOS.  Building deb...
fi

if [ $(uname -m) = x86_64 ]; then
  BIT_SIZE_NAME=x64
else
  BIT_SIZE_NAME=ia32
fi

if [[ "$RELEASE" == 1.9.32.* || "$RELEASE" == 1.10.33.* ]]; then
  # Without https://github.com/pagespeed/mod_pagespeed/commit/0e79a08 the font
  # inlining tests will fail, and we never backported that fix to 1.9 or 1.10.
  export DISABLE_FONT_API_TESTS=1
fi

build_dir="$HOME/build/$RELEASE/$BIT_SIZE_NAME"
release_dir="$HOME/release/$RELEASE/$BIT_SIZE_NAME"
log_dir="$build_dir/log"
rm -rf $log_dir
mkdir -p $log_dir
rm -rf "$release_dir"
mkdir -p "$release_dir"

# Usage:
#    check log_filename.log command args...
#
# The log file will placed in $log_dir, and will be 'tail'ed if the
# command fails.
function check() {
  # We are explicitly checking error status here and tailing the log
  # file so turn off auto-exit-on-error tempoararily.
  set +e
  log_filename="$log_dir/$1"
  shift
  echo "[$(date '+%k:%M:%S')] $@ >> $log_filename"
  echo $@ >> "$log_filename"
  $@ >> "$log_filename" 2>&1
  rc=$?
  if [ $rc != 0 ]; then
    echo '***' status is $rc
    tail "$log_filename"
    echo Failed at $(date)
    exit 1
  fi
  set -e
}

# We do the building in build/ which generates all kinds of crap which
# we don't ship in our binaries, such as .o and .a files.
#
# We put what we want to ship into release/ so that it can be scp'd onto
# the signing server with only one password.
mkdir -p $build_dir
rm -rf "$build_dir/src"
cd $build_dir && git clone https://github.com/pagespeed/mod_pagespeed.git src
cd $build_dir/src
if [ "$TAG" != "trunk" ]; then  # Just treat "trunk" as master.
  git checkout "$TAG"
fi

if [ "$EXT" = "rpm" ] ; then
  # MANYLINUX1 is required for CentOS 5 (but probably not newer CentOS):
  # https://github.com/grpc/grpc/issues/7147
  export CFLAGS='-DGPR_MANYLINUX1 -std=gnu99'

  # On the centos buildbot we need to patch the Makefile to make
  # apache_debug_restart to a killall -9 httpd.
  cd $build_dir/src
  killall_patch=$(cat <<EOF
diff --git a/install/Makefile b/install/Makefile
index 376def9..29fa72e 100644
--- a/install/Makefile
+++ b/install/Makefile
@@ -573,6 +573,7 @@ setup_test_machine :
 >sudo \$(APACHE_CONTROL_PROGRAM) restart

 apache_debug_restart :
+>killall -9 httpd || echo "not killed"
 >\$(APACHE_CONTROL_PROGRAM) restart

 apache_debug_stop : stop
EOF
)
  echo "$killall_patch" | tr '>' '\t' | git apply
fi
cd $build_dir

check gclient.log \
    gclient config https://github.com/pagespeed/mod_pagespeed.git --unmanaged --name=src
check gclient.log gclient sync --force

cd src

# Neither buildbot is using a compiler recent enough to provide stdalign.h,
# which boringssl needs.  Even on Centos 5's gcc 4.1 we do have a way to set
# alignment, though, so following
# https://sourceware.org/bugzilla/show_bug.cgi?id=19390 define alignas ourself
# and put it where boringssl can find it.
echo '#define alignas(x) __attribute__ ((aligned (x)))' > \
  third_party/boringssl/src/crypto/stdalign.h

if [ $do_patch -eq "1" ]; then
  echo Applying patch-file $patch_file
  git apply $patch_file

  echo "Re-running gclient in case the patch touched DEPS"
  check gclient.log gclient sync
fi

# This is needed on the vms, but not on our workstations for some reason.
find $build_dir/src -name "*.sh" | xargs chmod +x
cd $build_dir
echo src/build/gyp_chromium -Dchannel=$CHANNEL
export AR_host=$build_dir/src/build/wrappers/ar.sh
check gyp_chromium.log python src/build/gyp_chromium -Dchannel=$CHANNEL

cd src
# It would be better to have AR.target overridden at gyp time, but
# that functionality seems broken.
MODPAGESPEED_ENABLE_UPDATES=1 check build.log \
  make BUILDTYPE=Release AR.host=${AR_host} AR.target=${AR_host} V=1 \
    linux_package_$EXT mod_pagespeed_test pagespeed_automatic_test

ls -l $PWD/out/Release/mod-pagespeed-${CHANNEL}*
mkdir -p "$release_dir"
mv $PWD/out/Release/mod-pagespeed-${CHANNEL}* "$release_dir"

if [ "$EXT" = "rpm" ] ; then
  export SSL_CERT_DIR=/etc/pki/tls/certs
  export SSL_CERT_FILE=/etc/pki/tls/cert.pem
fi

check unit_test.log out/Release/mod_pagespeed_test
check unit_test.log out/Release/pagespeed_automatic_test

# Buildbots should have NOPASSWD set, so won't need to be prompted for sudo
# password.

echo Purging old releases ...
if [ "$EXT" = "rpm" ] ; then
  # rpm --erase only succeeds if all packages listed are installed, so we need
  # to find which one is installed and only erase that.
  rpm --query mod-pagespeed-stable mod-pagespeed-beta | \
      grep -v "is not installed" | \
      xargs --no-run-if-empty sudo rpm --erase
else
  # dpkg --purge succeeds even if one or both of the packages is not installed.
  sudo dpkg --purge mod-pagespeed-beta mod-pagespeed-stable
fi

echo Installing release ...
check install.log sudo $INSTALL "$release_dir"/*.$EXT

echo Test restart to make sure config file is valid ...
cd $build_dir/src/install
check install.log sudo -E $RESTART

echo Testing release ...
check system_test.log sudo -E $TEST

echo Copy the unstripped .so files to a safe place for easier debugging later.
NBITS=$(getconf LONG_BIT)
cp $build_dir/src/out/Release/libmod_pagespeed.so \
  "$release_dir"/unstripped_libmodpagespeed_${NBITS}_${EXT}.so
cp $build_dir/src/out/Release/libmod_pagespeed_ap24.so \
  "$release_dir"/unstripped_libmodpagespeed_ap24_${NBITS}_${EXT}.so

# Because we now build on the build-bots which are not on the internal network,
# you need to manually pull the builds from ~/release on each server.
echo Build succeeded at $(date)

# This doesn't necessarily need to be limited to CentOS, but we only need to
# build PSOL libraries on one system, and CentOS has the oldest GCC, so we
# build it there.
if [ "$EXT" = "rpm" -a "$CHANNEL" = "beta" ]; then
  echo Building PSOL binaries ...

  for buildtype in Release Debug; do
    cd $build_dir/src
    check psol_build.log make BUILDTYPE=$buildtype \
      AR.host=${AR_host} AR.target=${AR_host} V=1 \
      mod_pagespeed_test pagespeed_automatic_test


    if [[ "$RELEASE" == 1.9.32.* ]]; then
      # On 1.9 (and earlier, but we don't build them anymore) automatic/ was in
      # a different place.
      automatic_dir=net/instaweb/automatic/
    else
      automatic_dir=pagespeed/automatic/
    fi

    cd $automatic_dir

    # TODO(sligocki): Fix and use
    # check psol_automatic_build.log
    set +e
    make MOD_PAGESPEED_ROOT=$build_dir/src BUILDTYPE=$buildtype \
      AR.host=${AR_host} AR.target=${AR_host} V=1 \
      CXXFLAGS="-DSERF_HTTPS_FETCHING=1" \
      all \
      >> psol_automatic_build.log 2>&1
    set -e
    cd $build_dir/src

    BINDIR=$HOME/psol_release/$RELEASE/psol/lib/$buildtype/linux/$BIT_SIZE_NAME
    mkdir -p $BINDIR/
    mv $automatic_dir/pagespeed_automatic.a $BINDIR/
    if [ "$buildtype" = "Release" ]; then
      mv out/$buildtype/js_minify $BINDIR/pagespeed_js_minify
    fi

    # Sync release binaries incrementally as they're built so we don't
    # lose progress.
    echo PSOL $buildtype build succeeded at $(date)

    # VMs are running low on disk space, so clean up between builds.
    rm -rf out/$buildtype
  done
fi


exit 0
