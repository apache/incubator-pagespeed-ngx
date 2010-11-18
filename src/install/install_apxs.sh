#!/bin/bash
#
# Install Page Speed, using the Apache apxs tool to determine the
# installation locations.
#
# You can specify the path to apxs to install to a non-system-default
# Apache.
#
#  APXS_BIN=/path/to/apxs ./install_apxs.sh
#
# To install to a location that does not require superuser access:
#
#  NO_SUDO=1 ./install_apxs.sh

SRC_ROOT="$(dirname $0)/.."
BUILD_ROOT="${SRC_ROOT}/out/Release"
MODPAGESPEED_SO_PATH="${BUILD_ROOT}/libmod_pagespeed.so"

MODPAGESPEED_CACHE_ROOT=${MODPAGESPEED_CACHE_ROOT:-"/var/mod_pagespeed"}
APACHE_CONF_FILENAME=${APACHE_CONF_FILENAME:-"httpd.conf"}
MODPAGESPEED_SO_NAME=${MODPAGESPEED_SO_NAME:-"mod_pagespeed.so"}
MODPAGESPEED_CONF_NAME=${MODPAGESPEED_CONF_NAME:-"pagespeed.conf"}

MODPAGESPEED_FILE_USER=${MODPAGESPEED_FILE_USER:-"root"}
MODPAGESPEED_FILE_GROUP=${MODPAGESPEED_FILE_GROUP:-${MODPAGESPEED_FILE_USER}}
SUDO_CMD=${SUDO_CMD:-"sudo"}

# If NO_SUDO was specified, then we should use a user and group that
# matches the current user and group when installing files.
if [ ! -z "${NO_SUDO}" ]; then
  MODPAGESPEED_FILE_USER=$USER
  MODPAGESPEED_FILE_GROUP=$(groups | cut -d\  -f1)
  SUDO_CMD=""
fi

# Load the script used to perform template substitutions on our config
# files.
source ${SRC_ROOT}/install/common/installer.include

# Args: variable name
#
# Takes a variable name, and makes sure that it is set.
function is_set() {
  local DOLLAR='$';
  local TO_EVAL="${DOLLAR}$1"
  local VALUE=$(eval "echo $TO_EVAL")
  local RET=1
  if [ ! -z "${VALUE}" ]; then
    RET=0
  fi
  return $RET
}

# Args: variable name, expression, debug string
#
# Exits if the specified variable name does not have an assigned value
# or if the expression evaluates to false.
function check() {
  if ! is_set "$1" || ! eval "$2"; then
    echo "Unable to determine $3."
    echo "Please set the $1 environment variable when invoking $0."
    exit 1
  fi
}

# Args: user, group, misc, src, dst
#
# Some hackery to get around the fact that 'install' refuses to take
# owner/group arguments unless run as root.
function do_install() {
  local INST_USER_GROUP=""
  if [ -z "${NO_SUDO}" ]; then
    INST_USER_GROUP="-o $1 -g $2"
  fi
  eval "${SUDO_CMD} install $INST_USER_GROUP $3 $4 $5"
}

# Args: setting name
#
# Extract an Apache compile-time setting with the given name.
function extract_compile_setting() {
  EXTRACT_COMPILE_SETTING=
  APACHE_CONF_LINE=$(${APACHE_BIN} -V | grep $1)
  if [ ! -z "${APACHE_CONF_LINE}" ]; then
    local SED_REGEX="s/^.*${1}=?[\"\'\ ]*//"
    EXTRACTED_COMPILE_SETTING=$(echo "${APACHE_CONF_LINE}" |
        sed -r "${SED_REGEX}" |
        sed "s/[\"\'\ ]*$//")
  fi
}

if [ ! -f "${MODPAGESPEED_SO_PATH}" ]; then
  echo "${MODPAGESPEED_SO_PATH} doesn't exist. Need to build first."
  exit 1
fi

# Find the apxs binary, if not specified.
if [ -z "${APXS_BIN}" ]; then
  APXS_BIN=$(which apxs 2> /dev/null)
  if [ -z "${APXS_BIN}" ]; then
    APXS_BIN=$(which apxs2 2> /dev/null)
  fi
  if [ -z "${APXS_BIN}" ]; then
    # Default location when Apache is installed from source.
    APXS_BIN="/usr/local/apache2/bin/apxs"
  fi
fi

# Find apxs which tells us about the system.
check APXS_BIN "[ -f ${APXS_BIN} -a -x ${APXS_BIN} ]" "path to Apache apxs"

echo "Using ${APXS_BIN} to determine installation location."
echo ""

# This is an optional configuration variable. If set, the conf file
# path is relative to it.
APACHE_ROOT=$(${APXS_BIN} -q PREFIX)

# Find the Apache shared module dir.
APACHE_MODULEDIR=$(${APXS_BIN} -q LIBEXECDIR)
check APACHE_MODULEDIR "[ -d ${APACHE_MODULEDIR} ]" "Apache module dir"

# Find the Apache conf dir.
APACHE_CONFDIR=$(${APXS_BIN} -q SYSCONFDIR)
check APACHE_CONFDIR "[ -d ${APACHE_CONFDIR} ]" "Apache conf dir"

APACHE_SBINDIR=$(${APXS_BIN} -q SBINDIR)
check APACHE_SBINDIR "[ -d ${APACHE_SBINDIR} ]" "Apache bin dir"

APACHE_TARGET=$(${APXS_BIN} -q TARGET)
APACHE_BIN="${APACHE_SBINDIR}/${APACHE_TARGET}"
check APACHE_BIN "[ -f ${APACHE_BIN} -a -x ${APACHE_BIN} ]" "Apache binary"

# Find the Apache conf file.
if [ -z "${APACHE_CONF_FILE}" ]; then
  extract_compile_setting SERVER_CONFIG_FILE
  APACHE_CONF_FILE="${EXTRACTED_COMPILE_SETTING}"
fi
if [ ! -z "${APACHE_ROOT}" ]; then
  APACHE_CONF_FILE="${APACHE_ROOT}/${APACHE_CONF_FILE}"
fi
if [ -z "${APACHE_CONF_FILE}" ]; then
  APACHE_CONF_FILE="${APACHE_CONFDIR}/${APACHE_CONF_FILENAME}"
fi
check APACHE_CONF_FILE "[ -f ${APACHE_CONF_FILE} ]" "Apache configuration file"

# Try to grep for the Apache user.
if [ -z "${APACHE_USER}" ]; then
  APACHE_USER_LINE=$(egrep -i "^[[:blank:]]*User[[:blank:]]+" "${APACHE_CONF_FILE}")
  if [ ! -z "${APACHE_USER_LINE}" ]; then
    APACHE_USER=$(echo "${APACHE_USER_LINE}" |
        sed -r s/^.*User[[:blank:]]+[\"\']*// |
        sed s/[\"\'[:blank:]]*$//)
  fi
fi

# Try to grep for the Apache group.
if [ -z "${APACHE_GROUP}" ]; then
  APACHE_GROUP_LINE=$(egrep -i "^[[:blank:]]*Group[[:blank:]]+" "${APACHE_CONF_FILE}")
  if [ ! -z "${APACHE_GROUP_LINE}" ]; then
    APACHE_GROUP=$(echo "${APACHE_GROUP_LINE}" |
        sed -r s/^.*Group[[:blank:]]+[\"\']*// |
        sed s/[\"\'[:blank:]]*$//)
  fi
fi

# Make sure we have an Apache user and group.
check APACHE_USER "[ ! -z \'${APACHE_USER}\' ]" "Apache user"
check APACHE_GROUP "[ ! -z \'${APACHE_GROUP}\' ]" "Apache group"

# Make sure the user is valid.
check APACHE_USER "id '${APACHE_USER}' &> /dev/null" "valid Apache user '${APACHE_USER}'"

# Make sure the group is valid.
# TODO: is there a way to ask the system if a group exists, similar to
# the 'id' command?
check APACHE_GROUP "egrep -q '^${APACHE_GROUP}:' /etc/group" "valid Apache group '${APACHE_GROUP}'"

MODPAGESPEED_CONFDIR=${MODPAGESPEED_CONFDIR:-${APACHE_CONFDIR}}

echo "mod_pagespeed needs to cache optimized resources on the file system."
echo "The default location for this cache is '${MODPAGESPEED_CACHE_ROOT}'."
read -p "Would you like to specify a different location? (y/N) " -n1 PROMPT
if [ "${PROMPT}" = "y" -o "${PROMPT}" = "Y" ]; then
  echo ""
  read -p "Location for mod_pagespeed file cache: " MODPAGESPEED_CACHE_ROOT
fi

if [ -z "${MODPAGESPEED_CACHE_ROOT}" ]; then
  echo ""
  echo "Must specify a mod_pagespeed file cache."
  exit 1
fi

echo ""
echo "Preparing to install to the following locations:"
echo "${APACHE_MODULEDIR}/${MODPAGESPEED_SO_NAME} (${MODPAGESPEED_FILE_USER}:${MODPAGESPEED_FILE_GROUP})"
echo "${MODPAGESPEED_CONFDIR}/${MODPAGESPEED_CONF_NAME} (${MODPAGESPEED_FILE_USER}:${MODPAGESPEED_FILE_GROUP})"
echo "${MODPAGESPEED_CACHE_ROOT}/cache (${APACHE_USER}:${APACHE_GROUP})"
echo "${MODPAGESPEED_CACHE_ROOT}/files (${APACHE_USER}:${APACHE_GROUP})"
echo ""
if [ -z "${NO_PROMPT}" ]; then
  echo -n "Continue? (y/N) "
  read -n1 PROMPT
  echo ""
  if [ "${PROMPT}" != "y" -a "${PROMPT}" != "Y" ]; then
    echo "Not continuing."
    exit 1
  fi
fi

if [ -d "${MODPAGESPEED_CACHE_ROOT}/cache" ]; then
  echo "${MODPAGESPEED_CACHE_ROOT}/cache already exists. Not creating."
fi
if [ -d "${MODPAGESPEED_CACHE_ROOT}/files" ]; then
  echo "${MODPAGESPEED_CACHE_ROOT}/files already exists. Not creating."
fi

# Only attempt to load mod_deflate in our conf file if it's actually
# present on the system.
COMMENT_OUT_DEFLATE='\#'
if [ -f "${APACHE_MODULEDIR}/mod_deflate.so" ]; then
  COMMENT_OUT_DEFLATE=
else
  echo "Unable to find mod_deflate.so. HTTP compression support not enabled!"
fi

TMP_CONF=$(mktemp -t conf.tmp.XXXXXX) || exit 1
process_template "${SRC_ROOT}/install/common/pagespeed.conf.template" "${TMP_CONF}"
TMP_LOAD=$(mktemp -t load.tmp.XXXXXX) || exit 1
process_template "${SRC_ROOT}/install/common/pagespeed.load.template" "${TMP_LOAD}"
cat "${TMP_CONF}" >> "${TMP_LOAD}"

INSTALLATION_SUCCEEDED=0
if (
do_install "${MODPAGESPEED_FILE_USER}" "${MODPAGESPEED_FILE_GROUP}" "-m 644 -s" \
  "${MODPAGESPEED_SO_PATH}" \
  "${APACHE_MODULEDIR}/${MODPAGESPEED_SO_NAME}" &&
do_install "${MODPAGESPEED_FILE_USER}" "${MODPAGESPEED_FILE_GROUP}" "-m 644" \
  "${TMP_LOAD}" \
  "${MODPAGESPEED_CONFDIR}/${MODPAGESPEED_CONF_NAME}" &&
do_install "${APACHE_USER}" "${APACHE_GROUP}" "-m 755 -d" \
  "${MODPAGESPEED_CACHE_ROOT}/cache" \
  "${MODPAGESPEED_CACHE_ROOT}/files"
); then
  MODPAGESPEED_LOAD_LINE="Include ${MODPAGESPEED_CONFDIR}/${MODPAGESPEED_CONF_NAME}"
  if ! grep -q "${MODPAGESPEED_LOAD_LINE}" "${APACHE_CONF_FILE}"; then
    echo "Adding a load line for mod_pagespeed to ${APACHE_CONF_FILE}."
    ${SUDO_CMD} sh -c "echo ${MODPAGESPEED_LOAD_LINE} >> ${APACHE_CONF_FILE}"
  fi
  if grep -q "${MODPAGESPEED_LOAD_LINE}" "${APACHE_CONF_FILE}"; then
    INSTALLATION_SUCCEEDED=1
  fi
fi

echo ""
if [ $INSTALLATION_SUCCEEDED -eq 1 ]; then
  echo "Installation succeeded."
  echo "Restart apache to enable mod_pagespeed."
else
  echo "Installation failed."
fi

rm -f "${TMP_CONF}" "${TMP_LOAD}"

