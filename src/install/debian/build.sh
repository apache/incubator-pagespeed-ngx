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

# Create the Debian changelog file needed by dpkg-gencontrol. This just adds a
# placeholder change, indicating it is the result of an automatic build.
gen_changelog() {
  rm -f "${DEB_CHANGELOG}"
  process_template "${SCRIPTDIR}/changelog.template" "${DEB_CHANGELOG}"
  debchange -a --nomultimaint -m --changelog "${DEB_CHANGELOG}" \
    --distribution UNRELEASED "automatic build"
}

# Create the Debian control file needed by dpkg-deb.
gen_control() {
  dpkg-gencontrol -v"${VERSIONFULL}" -c"${DEB_CONTROL}" -l"${DEB_CHANGELOG}" \
  -f"${DEB_FILES}" -p"${PACKAGE}-${CHANNEL}" -P"${STAGEDIR}" -T"${DEB_SUBST}" \
  -O > "${STAGEDIR}/DEBIAN/control"
  rm -f "${DEB_CONTROL}"
}

# Create the Debian substvars file needed by dpkg-gencontrol.
gen_substvars() {
  # dpkg-shlibdeps requires a control file in debian/control, so we're
  # forced to prepare a fake debian directory.
  mkdir "${SUBSTFILEDIR}/debian"
  cp "${DEB_CONTROL}" "${SUBSTFILEDIR}/debian"
  pushd "${SUBSTFILEDIR}" >/dev/null
  dpkg-shlibdeps "${STAGEDIR}${APACHE_MODULEDIR}/mod_pagespeed.so" \
  -O >> "${DEB_SUBST}" 2>/dev/null
  dpkg-shlibdeps "${STAGEDIR}${APACHE_MODULEDIR}/mod_pagespeed_ap24.so" \
  -O >> "${DEB_SUBST}" 2>/dev/null
  popd >/dev/null
}

# Setup the installation directory hierachy in the package staging area.
prep_staging_debian() {
  prep_staging_common
  install -m 755 -d "${STAGEDIR}/DEBIAN" \
    "${STAGEDIR}/etc/cron.daily" \
    "${STAGEDIR}/etc/apache2/conf.d" \
    "${STAGEDIR}/usr/bin"
}

# Put the package contents in the staging area.
stage_install_debian() {
  prep_staging_debian
  stage_install_common
  echo "Staging Debian install files in '${STAGEDIR}'..."
  process_template "${BUILDDIR}/install/common/repo.cron" \
    "${STAGEDIR}/etc/cron.daily/${PACKAGE}"
  chmod 755 "${STAGEDIR}/etc/cron.daily/${PACKAGE}"
  process_template "${BUILDDIR}/install/debian/postinst" \
    "${STAGEDIR}/DEBIAN/postinst"
  chmod 755 "${STAGEDIR}/DEBIAN/postinst"
  process_template "${BUILDDIR}/install/debian/prerm" \
    "${STAGEDIR}/DEBIAN/prerm"
  chmod 755 "${STAGEDIR}/DEBIAN/prerm"
  process_template "${BUILDDIR}/install/debian/postrm" \
    "${STAGEDIR}/DEBIAN/postrm"
  chmod 755 "${STAGEDIR}/DEBIAN/postrm"
  install -m 644 "${BUILDDIR}/install/debian/conffiles" \
    "${STAGEDIR}/DEBIAN/conffiles"
  echo "/etc/cron.daily/${PACKAGE}" >> "${STAGEDIR}/DEBIAN/conffiles"
  process_template "${BUILDDIR}/install/common/pagespeed.load.template" \
    "${STAGEDIR}${APACHE_CONFDIR}/pagespeed.load"
  chmod 644 "${STAGEDIR}${APACHE_CONFDIR}/pagespeed.load"
  process_template "${BUILDDIR}/install/common/pagespeed.conf.template" \
    "${STAGEDIR}${APACHE_CONFDIR}/pagespeed.conf"
  install -m 755 "${BUILDDIR}/js_minify" \
    "${STAGEDIR}/usr/bin/pagespeed_js_minify"
  chmod 644 "${STAGEDIR}${APACHE_CONFDIR}/pagespeed.conf"
  install -m 644 \
    "${BUILDDIR}/../../net/instaweb/genfiles/conf/pagespeed_libraries.conf" \
    "${STAGEDIR}${APACHE_CONF_D_DIR}/pagespeed_libraries.conf"
}

# Build the deb file within a fakeroot.
do_package_in_fakeroot() {
  FAKEROOTFILE=$(mktemp -t fakeroot.tmp.XXXXXX) || exit 1
  fakeroot -s "${FAKEROOTFILE}" -- \
    chown -R ${APACHE_USER}:${APACHE_USER} ${STAGEDIR}${MOD_PAGESPEED_CACHE}
  fakeroot -s "${FAKEROOTFILE}" -i "${FAKEROOTFILE}" -- \
    chown -R ${APACHE_USER}:${APACHE_USER} ${STAGEDIR}${MOD_PAGESPEED_LOG}
  fakeroot -i "${FAKEROOTFILE}" -- \
    dpkg-deb -b "${STAGEDIR}" .
  rm -f "${FAKEROOTFILE}"
}

# Actually generate the package file.
do_package() {
  export HOST_ARCH="$1"
  echo "Packaging ${HOST_ARCH}..."
  PREDEPENDS="$COMMON_PREDEPS"
  DEPENDS="${COMMON_DEPS}"

  # Generate Conflicts: and Replaces: headers for the other channel to get
  # dpkg to seamlessly switch channels on -i
  case $CHANNEL in
  stable )
    CONFLICTS=mod-pagespeed-beta
    ;;
  beta )
    CONFLICTS=mod-pagespeed-stable
    ;;
  * )
    echo
    echo "ERROR: '$CHANNEL' is not a valid channel type."
    echo
    exit 1
    ;;
  esac
  REPLACES="${CONFLICTS}"

  gen_changelog
  process_template "${SCRIPTDIR}/control.template" "${DEB_CONTROL}"
  export DEB_HOST_ARCH="${HOST_ARCH}"
  gen_substvars
  if [ -f "${DEB_CONTROL}" ]; then
    gen_control
  fi

  do_package_in_fakeroot
}

# Remove temporary files and unwanted packaging output.
cleanup() {
  echo "Cleaning..."
  rm -rf "${STAGEDIR}"
  rm -rf "${TMPFILEDIR}"
  rm -rf "${SUBSTFILEDIR}"
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
      ;;
    testing|beta )
      CHANNEL=beta
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
STAGEDIR=$(mktemp -d -t deb.build.XXXXXX) || exit 1
TMPFILEDIR=$(mktemp -d -t deb.tmp.XXXXXX) || exit 1
SUBSTFILEDIR=$(mktemp -d -t deb.subst.XXXXXX) || exit 1
DEB_CHANGELOG="${TMPFILEDIR}/changelog"
DEB_FILES="${TMPFILEDIR}/files"
DEB_CONTROL="${TMPFILEDIR}/control"
DEB_SUBST="${SUBSTFILEDIR}/debian/substvars"
CHANNEL="beta"
# Default target architecture to same as build host.
if [ "$(uname -m)" = "x86_64" ]; then
  TARGETARCH="x64"
else
  TARGETARCH="ia32"
fi

# call cleanup() on exit
trap cleanup 0
process_opts "$@"
if [ ! "$BUILDDIR" ]; then
  BUILDDIR=$(readlink -f "${SCRIPTDIR}/../../out/Release")
fi

source ${BUILDDIR}/install/common/installer.include

get_version_info
VERSIONFULL="${VERSION}-r${REVISION}"

source "${BUILDDIR}/install/common/mod-pagespeed.info"
eval $(sed -e "s/^\([^=]\+\)=\(.*\)$/export \1='\2'/" \
  "${BUILDDIR}/install/common/BRANDING")

REPOCONFIG="deb http://dl.google.com/linux/${PACKAGE#google-}/deb/ stable main"
verify_channel

# Some Debian packaging tools want these set.
export DEBFULLNAME="${MAINTNAME}"
export DEBEMAIL="${MAINTMAIL}"

# Make everything happen in the OUTPUTDIR.
cd "${OUTPUTDIR}"

COMMON_DEPS="apache2.2-common|apache2-api-20120211"
COMMON_PREDEPS="dpkg (>= 1.14.0)"

APACHE_MODULEDIR="/usr/lib/apache2/modules"
APACHE_CONFDIR="/etc/apache2/mods-available"
APACHE_CONF_D_DIR="/etc/apache2/conf.d"
MOD_PAGESPEED_CACHE="/var/cache/mod_pagespeed"
MOD_PAGESPEED_LOG="/var/log/pagespeed"
APACHE_USER="www-data"
COMMENT_OUT_DEFLATE=
SSL_CERT_DIR="/etc/ssl/certs"
SSL_CERT_FILE_COMMAND=

case "$TARGETARCH" in
  ia32 )
    stage_install_debian
    do_package "i386"
    ;;
  x64 )
    stage_install_debian
    do_package "amd64"
    ;;
  * )
    echo
    echo "ERROR: Don't know how to build DEBs for '$TARGETARCH'."
    echo
    exit 1
    ;;
esac
