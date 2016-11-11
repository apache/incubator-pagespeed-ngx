#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Install packages required for building mod_pagespeed.

if [ "$UID" -ne 0 ]; then
  echo Root is required to run this. Re-execing with sudo
  exec sudo $0 "$@"
  exit 1  # NOTREACHED
fi

additional_test_packages=false
if [ "${1:-}" = "--additional_test_packages" ]; then
  additional_test_packages=true
  shift
fi

if [ $# -ne 0 ]; then
  echo "Usage: $(basename $0) [--additional_test_packages]" >&2
  exit 1
fi

binary_packages=(subversion httpd gcc-c++ gperf make rpm-build
  glibc-devel at curl-devel expat-devel gettext-devel openssl-devel zlib-devel
  libevent-devel rsync redhat-lsb)
src_packages=()

if "$additional_test_packages"; then
  binary_packages+=(php php-mbstring)
  src_packages+=(redis-server)
fi

# Which distribution of Scientific Linux gcc to install (5 or 6).
install_sl_gcc=

if version_compare "$(lsb_release -rs)" -ge 7; then
  binary_packages+=(python27 wget git)
  if "$additional_test_packages"; then
    binary_packages+=(memcached)
  fi
elif version_compare "$(lsb_release -rs)" -ge 6; then
  install_sl_gcc=6
  binary_packages+=(python26 wget)
  # gyp runs "git rev-list --all --count" which the CentOS 6 package is too old
  # for.
  src_packages+=(git)
  if "$additional_test_packages"; then
    binary_packages+=(memcached)
  fi
else
  install_sl_gcc=5
  # Note that wget of git doesn't work on CentOS 5 due to it having an ancient
  # OpenSSL. You need to manually scp up the contents of $GIT_SRC_URL from
  # shell_utils.sh.
  src_packages+=(python2.7 wget git)
  if "$additional_test_packages"; then
    src_packages+=(memcached)
  fi
fi

# Are we installing gcc from Scientific Linux?
if [ -n "$install_sl_gcc" ]; then
  # The signing cert is the same for all versions.
  curl -o /etc/pki/rpm-gpg/RPM-GPG-KEY-cern \
    https://linux.web.cern.ch/linux/scientific6/docs/repository/cern/slc6X/i386/RPM-GPG-KEY-cern
  rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-cern
  # We have to use curl; wget can't parse their SAN.
  curl -o /etc/yum.repos.d/slc${install_sl_gcc}-devtoolset.repo \
    https://linux.web.cern.ch/linux/scientific${install_sl_gcc}/docs/repository/cern/devtoolset/slc${install_sl_gcc}-devtoolset.repo
  binary_packages+=(devtoolset-2-gcc-c++ devtoolset-2-binutils)
fi

yum -y install "${binary_packages[@]}"

# Make sure atd started after installation.
/etc/init.d/atd start || true

# To build on Centos 5/6 we need gcc 4.8 from Scientific Linux.  We can't
# export CC and CXX because some steps still use a literal "g++", but #$%^
# devtoolset includes its own sudo, and we don't want that because it doesn't
# support -E, so rename it if it exists.
DEVTOOLSET_BIN=/opt/rh/devtoolset-2/root/usr/bin/
if [ -e "$DEVTOOLSET_BIN/sudo" ]; then
  mv "$DEVTOOLSET_BIN/sudo" "$DEVTOOLSET_BIN/sudo.ignored"
fi

# At least CentOS 5 puts the Include directives before it sets the LogLevel,
# but we set LogLevel in pagespeed.conf because some of our tests depend on it.
# If httpd.conf sets LogLevel after any includes, move the LogLevel before the
# first Include.
httpd_conf=/etc/httpd/conf/httpd.conf
include_line_number="$(grep -En '^Include[[:space:]]' $httpd_conf | cut -d: -f 1 | head -n 1)"
loglevel_plus_line_number="$(grep -En '^LogLevel[[:space:]]' $httpd_conf | tail -n 1)"
loglevel_line_number="${loglevel_plus_line_number%%:*}"

if [ -n "$include_line_number" -a -n "$loglevel_line_number" ] && \
   [ "$include_line_number" -lt "$loglevel_line_number" ]; then
  loglevel_line="${loglevel_plus_line_number#*:}"
  # Comment out all LoglLevel lines but insert the last one found before the first Include.
  sed -i.pagespeed_bak "s/^LogLevel[[:space:]]/#&/; 0,/^Include/s//$loglevel_line\\n&/" $httpd_conf
fi

install_from_src "${src_packages[@]}"

# Start memcached if it was installed from source
# TODO(cheesy): This should probably happen as part of the test setup, though
# the tests expect it to have been started by initscripts.
if [ -e /usr/local/bin/memcached ]; then
  /usr/local/bin/memcached -d -u nobody -m 512 -p 11211 127.0.0.1
fi
