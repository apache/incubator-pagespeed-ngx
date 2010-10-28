#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
if [ "$VERBOSE" ]; then
  set -x
fi
set -u

gen_spec() {
  rm -f "${SPEC}"
  process_template "${SCRIPTDIR}/mod-pagespeed.spec.template" "${SPEC}"
}

# Setup the installation directory hierachy in the package staging area.
prep_staging_rpm() {
  prep_staging_common
  install -m 755 -d "${STAGEDIR}/etc/cron.daily"
}

# Put the package contents in the staging area.
stage_install_rpm() {
  prep_staging_rpm
  stage_install_common
  echo "Staging RPM install files in '${STAGEDIR}'..."
  process_template "${BUILDDIR}/install/common/rpmrepo.cron" \
    "${STAGEDIR}/etc/cron.daily/${PACKAGE}"
  chmod 755 "${STAGEDIR}/etc/cron.daily/${PACKAGE}"

  # For CentOS, the load and conf files are combined into a single
  # 'conf' file. So we install the load template as the conf file, and
  # then concatenate the actual conf file.
  process_template "${BUILDDIR}/install/common/pagespeed.load.template" \
    "${STAGEDIR}${APACHE_CONFDIR}/pagespeed.conf"
  process_template "${BUILDDIR}/install/common/pagespeed.conf.template" \
    "${BUILDDIR}/install/common/pagespeed.conf"
  cat "${BUILDDIR}/install/common/pagespeed.conf" >> \
    "${STAGEDIR}${APACHE_CONFDIR}/pagespeed.conf"
  chmod 644 "${STAGEDIR}${APACHE_CONFDIR}/pagespeed.conf"
}

# Actually generate the package file.
do_package() {
  echo "Packaging ${HOST_ARCH}..."
  PROVIDES="${PACKAGE}"
  local REPS="$REPLACES"
  REPLACES=""
  for rep in $REPS; do
    if [ -z "$REPLACES" ]; then
      REPLACES="$PACKAGE-$rep"
    else
      REPLACES="$REPLACES $PACKAGE-$rep"
    fi
  done

  # If we specify a dependecy of foo.so below, we would depend on both the
  # 32 and 64-bit versions on a 64-bit machine. The current version of RPM
  # we use is too old and doesn't provide %{_isa}, so we do this manually.
  if [ "$HOST_ARCH" = "x86_64" ] ; then
    local EMPTY_VERSION="()"
    local PKG_ARCH="(64bit)"
  elif [ "$HOST_ARCH" = "i386" ] ; then
    local EMPTY_VERSION=""
    local PKG_ARCH=""
  fi

  DEPENDS="httpd >= 2.2, \
  libstdc++ >= 4.1.2, \
  at"
  gen_spec

  # Create temporary rpmbuild dirs.
  RPMBUILD_DIR=$(mktemp -d -t rpmbuild.XXXXXX) || exit 1
  mkdir -p "$RPMBUILD_DIR/BUILD"
  mkdir -p "$RPMBUILD_DIR/RPMS"

  rpmbuild --buildroot="$RPMBUILD_DIR/BUILD" -bb \
    --target="$HOST_ARCH" --rmspec \
    --define "_topdir $RPMBUILD_DIR" \
    --define "_binary_payload w9.bzdio" \
    "${SPEC}"
  PKGNAME="${PACKAGE}-${CHANNEL}-${VERSION}-${REVISION}"
  mv "$RPMBUILD_DIR/RPMS/$HOST_ARCH/${PKGNAME}.${HOST_ARCH}.rpm" "${OUTPUTDIR}"
  # Make sure the package is world-readable, otherwise it causes problems when
  # copied to share drive.
  chmod a+r "${OUTPUTDIR}/${PKGNAME}.$HOST_ARCH.rpm"
  rm -rf "$RPMBUILD_DIR"
}

# Remove temporary files and unwanted packaging output.
cleanup() {
  rm -rf "${STAGEDIR}"
  rm -rf "${TMPFILEDIR}"
}

usage() {
  echo "usage: $(basename $0) [-c channel] [-a target_arch] [-o 'dir'] [-b 'dir']"
  echo "-c channel the package channel (unstable, beta, stable)"
  echo "-a arch    package architecture (ia32 or x64)"
  echo "-o dir     package output directory [${OUTPUTDIR}]"
  echo "-b dir     build input directory    [${BUILDDIR}]"
  echo "-h         this help message"
}

# Check that the channel name is one of the allowable ones.
verify_channel() {
  case $CHANNEL in
    stable )
      CHANNEL=stable
      REPLACES="unstable beta"
      ;;
    unstable|dev|alpha )
      CHANNEL=unstable
      REPLACES="stable beta"
      ;;
    testing|beta )
      CHANNEL=beta
      REPLACES="unstable stable"
      ;;
    * )
      echo
      echo "ERROR: '$CHANNEL' is not a valid channel type."
      echo
      exit 1
      ;;
  esac
}

process_opts() {
  while getopts ":o:b:c:a:h" OPTNAME
  do
    case $OPTNAME in
      o )
        OUTPUTDIR="$OPTARG"
        mkdir -p "${OUTPUTDIR}"
        ;;
      b )
        BUILDDIR=$(readlink -f "${OPTARG}")
        ;;
      c )
        CHANNEL="$OPTARG"
        verify_channel
        ;;
      a )
        TARGETARCH="$OPTARG"
        ;;
      h )
        usage
        exit 0
        ;;
      \: )
        echo "'-$OPTARG' needs an argument."
        usage
        exit 1
        ;;
      * )
        echo "invalid command-line option: $OPTARG"
        usage
        exit 1
        ;;
    esac
  done
}

#=========
# MAIN
#=========

SCRIPTDIR=$(readlink -f "$(dirname "$0")")
OUTPUTDIR="${PWD}"
STAGEDIR=$(mktemp -d -t rpm.build.XXXXXX) || exit 1
TMPFILEDIR=$(mktemp -d -t rpm.tmp.XXXXXX) || exit 1
CHANNEL="beta"
# Default target architecture to same as build host.
if [ "$(uname -m)" = "x86_64" ]; then
  TARGETARCH="x64"
else
  TARGETARCH="ia32"
fi
SPEC="${TMPFILEDIR}/mod-pagespeed.spec"

# call cleanup() on exit
trap cleanup 0
process_opts "$@"
if [ ! "$BUILDDIR" ]; then
  BUILDDIR=$(readlink -f "${SCRIPTDIR}/../../out/Release")
fi

source ${BUILDDIR}/install/common/installer.include

get_version_info

source "${BUILDDIR}/install/common/mod-pagespeed.info"
eval $(sed -e "s/^\([^=]\+\)=\(.*\)$/export \1='\2'/" \
  "${BUILDDIR}/install/common/BRANDING")

REPOCONFIG="http://dl.google.com/linux/${PACKAGE#google-}/rpm/stable"
verify_channel

APACHE_MODULEDIR="/usr/lib/httpd/modules"
APACHE_CONFDIR="/etc/httpd/conf.d"
MODPAGESPEED_CACHE_ROOT="/var/mod_pagespeed"
APACHE_USER="apache"

# Make everything happen in the OUTPUTDIR.
cd "${OUTPUTDIR}"

case "$TARGETARCH" in
  ia32 )
    export HOST_ARCH="i386"
    stage_install_rpm
    ;;
  x64 )
    export HOST_ARCH="x86_64"
    stage_install_rpm
    ;;
  * )
    echo
    echo "ERROR: Don't know how to build RPMs for '$TARGETARCH'."
    echo
    exit 1
    ;;
esac

do_package "$HOST_ARCH"
