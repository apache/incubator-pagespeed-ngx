#!/bin/bash
#
# Copyright 2016 Google Inc.
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
# Author: cheesy@google.com (Steve Hill)
#
# Install packages required for building mod_pagespeed.

if [ "$UID" -ne 0 ]; then
  echo Root is required to run this. Re-execing with sudo
  exec sudo $0 "$@"
  exit 1  # NOTREACHED
fi

additional_dev_packages=false
if [ "${1:-}" = "--additional_dev_packages" ]; then
  additional_dev_packages=true
  shift
fi

if [ $# -ne 0 ]; then
  echo "Usage: $(basename $0) [--additional_dev_packages]" >&2
  exit 1
fi

binary_packages=(subversion apache2 g++ gperf devscripts fakeroot git-core
  zlib1g-dev wget curl net-tools rsync ssl-cert psmisc)
src_packages=()

if version_compare $(lsb_release -rs) -lt 14.04; then
  binary_packages+=(gcc-mozilla)
fi

# Sometimes the names of packages change between versions.  This goes through
# its arguments and returns the first package name that exists on this OS.
function first_available_package() {
  for candidate_version in "$@"; do
    if [ -n "$(apt-cache search --names-only "^${candidate_version}$")" ]; then
      echo "$candidate_version"
      return
    fi
  done
  echo "error: no available version of $@" >&2
  exit 1
}

install_redis_from_src=false
if "$additional_dev_packages"; then
  binary_packages+=(memcached autoconf valgrind libev-dev libssl-dev
    libpcre3-dev openjdk-7-jre language-pack-tr-base gperf uuid-dev)

  if version_compare $(lsb_release -sr) -ge 16.04; then
    binary_packages+=(redis-server)
  else
    src_packages+=(redis-server)
  fi

  binary_packages+=( \
    $(first_available_package libtool-bin libtool)
    $(first_available_package php-cgi php5-cgi)
    $(first_available_package libapache2-mod-php libapache2-mod-php5)
    $(first_available_package php-mbstring libapache2-mod-php5))
fi

apt-get -y install "${binary_packages[@]}"

# src_packages might be empty. The below placates set -u, see:
# http://stackoverflow.com/questions/7577052/bash-empty-array-expansion-with-set-u
install_from_src ${src_packages[@]+"${src_packages[@]}"}
