#!/bin/bash
#
# Copyright 2012 Google Inc.
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
# Script to enable PHP5 in Apache assuming it has already been installed into
# the standard Ubuntu directory (/etc/apache, /usr/apache) rather than the
# ~/apache2 directory we use [in other words, modules have been apt install'd
# rather than built from source].
#
# PHP5 can be installed using the following commands:
#  apt-get install php5-common php5
#  apt-get install php5-cgi php5-cli libapache2-mod-fcgid # * worker MPM only *
#
# Note that it does not fail if any of these are not installed since we don't
# want to force site admins to install them just to run some tests.
#
# usage: apache_configure_php5_from_etc_php5.sh apache-root-directory
APACHE_ROOT=$1
if [ -z "${APACHE_ROOT}" ]; then
  echo "Usage: $0 <apache-root-directory>"
  exit 1
fi

HTTPD_CONF=${APACHE_ROOT}/conf/httpd.conf
DST_PHP5_CONFIG=${APACHE_ROOT}/conf/php5.conf
DST_PHP5_MODULE=${APACHE_ROOT}/modules/libphp5.so
DST_FCGID_CONFIG=${APACHE_ROOT}/conf/fcgid.conf
DST_FCGID_MODULE=${APACHE_ROOT}/modules/mod_fcgid.so

# Note: contains an embedded TAB.
WS="[ 	]"

# Early exit if everything seems to be installed already.
grep -q "^${WS}*Include${WS}.*conf/php5.conf${WS}*$" "${HTTPD_CONF}" && \
grep -q "^${WS}*Include${WS}.*conf/fcgid.conf${WS}*$" "${HTTPD_CONF}"
if [[ $? -eq 0 && \
     -r "${DST_PHP5_CONFIG}" && \
     -r "${DST_PHP5_MODULE}" && \
     -r "${DST_FCGID_MODULE}" ]]; then
  exit 0
fi

# Hardwire where we get things from since it's a Ubuntu standard.
SRC_PHP5_INIDIR=/etc/php5/apache2
SRC_PHP5_MODULE=/usr/lib/apache2/modules/libphp5.so

# We want our own build of fcgid since we want to test with 2.2, while the
# packages are for 2.4
SRC_FCGID_MODULE=${APACHE_ROOT}/modules/mod_fcgid-src_build.so

# Bail if PHP5 isn't installed [where we expect it].

if [[ ! -r "/usr/bin/php5-cgi" ||
      ! -r "${SRC_FCGID_MODULE}" ]]; then
  echo "*** PHP5 is not installed, or is not installed where we expect" >&2
  echo "    under /etc/php5 and /usr/lib/apache2. Please run:"          >&2
  echo "        sudo apt-get install php5-common php5"                  >&2
  echo "        sudo apt-get install php5-cgi php5-cli libapache2-mod-fcgid">&2
  echo ""
  echo "        You may also need to rm -rf ${APACHE_ROOT}" >&2
  echo "        and re-run install/build_development_apache.sh"
  exit 1
fi

# Tricky grep'ing to find the correct Directory section in httpd.conf:
PAT1="<Directory${WS}${WS}*${APACHE_ROOT}/htdocs${WS}*>"
PAT2="<Directory${WS}${WS}*\"${APACHE_ROOT}/htdocs\"${WS}*>"
HTDOCS_OPEN_LINENO=$(
    egrep -n "${PAT1}|${PAT2}" "${HTTPD_CONF}" \
  | sed -e 's/:.*//')
if [ -z "$HTDOCS_OPEN_LINENO" ]; then
  echo
  echo "*** ${HTTPD_CONF} does not have a line like:"                       >&2
  echo '        <Directory "'${APACHE_ROOT}/htdocs'">'                  >&2
  echo "    which is the expected document root for the installation"   >&2
  echo "    and whose entry needs to be updated. ABORTING."             >&2
  exit 1
fi
HTDOCS_CLOSE_LINENO=$(
    tail -n +${HTDOCS_OPEN_LINENO} "${HTTPD_CONF}" \
  | grep -n "^${WS}*</${WS}*Directory${WS}*>" \
  | head -1 \
  | sed -e 's/:.*//')
OPTIONS_LINENO=$(
    tail -n +${HTDOCS_OPEN_LINENO} "${HTTPD_CONF}" \
  | head -${HTDOCS_CLOSE_LINENO:-999999} \
  | grep -i "^${WS}${WS}*Options${WS}.*[+]\?ExecCGI")
if [ -z "$OPTIONS_LINENO" ]; then
   [ -n "${HTDOCS_CLOSE_LINENO}" ] && \
   HTDOCS_CLOSE_LINENO=$((HTDOCS_OPEN_LINENO + HTDOCS_CLOSE_LINENO - 1))
   sed -e "${HTDOCS_CLOSE_LINENO:-$}"'i\
    # Required for mod_fcgi which is required for PHP when using worker MPM.\
    Options +ExecCGI' "${HTTPD_CONF}" > "${HTTPD_CONF}".tmp
  mv "${HTTPD_CONF}".tmp "${HTTPD_CONF}"
fi

# Add the necessary lines to httpd.conf if/as necessary.
fgrep -q "LoadModule fcgid_module modules/mod_fcgid.so" "${HTTPD_CONF}"
if [ $? -ne 0 ]; then
  # Backwards compatibility: check if PHP5 for Apache 2.2 prefork is setup.
  grep -q "^${WS}*LoadModule${WS}${WS}*php5_module${WS}.*modules/libphp5.so${WS}*$" "${HTTPD_CONF}"
  if [ $? -eq 0 ]; then
    # Remove the lines that just setup PHP5 for APache 2.2 prefork.
    sed -e "/^${WS}*LoadModule${WS}${WS}*php5_module${WS}.*/d" \
        -e "/^${WS}*Include${WS}${WS}*conf\/php5.conf${WS}*$/d" \
        "${HTTPD_CONF}" > "${HTTPD_CONF}.tmp"
    mv "${HTTPD_CONF}.tmp" "${HTTPD_CONF}"
  fi
  # Insert the all-singing all dancing lines for Apache 2.2 prefork/worker.
  # Unconditionally use mod_fcgid.
  cat - >> "${HTTPD_CONF}" <<EOF
  LoadModule fcgid_module modules/mod_fcgid.so
  Include conf/fcgid.conf
  AddHandler fcgid-script .php
  FCGIWrapper /usr/bin/php-cgi .php
EOF
fi

# Copy the config files over as necessary.
if [ ! -f "${DST_PHP5_CONFIG}" ]; then
  cat - > "${DST_PHP5_CONFIG}" <<EOF
<IfModule php5_module>
  AddHandler php5-script .php
  DirectoryIndex index.html index.php
  AddType text/html .php
  AddType application/x-httpd-php-source phps
  PHPIniDir ${SRC_PHP5_INIDIR}
</IfModule>
EOF
fi

if [ ! -f "${DST_FCGID_CONFIG}" ]; then
  cat - > "${DST_FCGID_CONFIG}" <<EOF
<IfModule mod_fcgid.c>
  AddHandler fcgid-script .fcgi
  FcgidConnectTimeout 20
  FcgidProcessTableFile fcgid/fcgid_shm
  FcgidIPCDir fcgid/sock
</IfModule>
EOF
fi

# Link the modules as necessary.
if [[ ! -f "${DST_PHP5_MODULE}" && -f "${SRC_PHP5_MODULE}" ]]; then
  ln -s "${SRC_PHP5_MODULE}" "${DST_PHP5_MODULE}"
fi
if [ ! -f "${DST_FCGID_MODULE}" ]; then
  ln -s "${SRC_FCGID_MODULE}" "${DST_FCGID_MODULE}"
fi

# Create the mod_fcgid directories.
[ -d "${APACHE_ROOT}"/fcgid ] || mkdir "${APACHE_ROOT}"/fcgid
[ -d "${APACHE_ROOT}"/fcgid/sock ] || mkdir "${APACHE_ROOT}"/fcgid/sock

exit 0
