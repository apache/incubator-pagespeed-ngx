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
# Sets common environment variables requires for building PageSpeed stuff.

devtoolset_dir=/opt/rh/devtoolset-2
devtoolset_enable="${devtoolset_dir}/enable"
if [ -f "$devtoolset_enable" ]; then
  # devtoolset_enable uses possibly unset vars, so disable set -u for it.
  set +u
  source "$devtoolset_enable"
  set -u
  : ${CC:="${devtoolset_dir}/root/usr/bin/gcc"}
  : ${CXX:="${devtoolset_dir}/root/usr/bin/g++"}
  export CC CXX

fi

export CHROOT_DIR=/var/chroot/centos_i386

export SSL_CERT_DIR=/etc/pki/tls/certs
export SSL_CERT_FILE=/etc/pki/tls/cert.pem

export CFLAGS="-DGPR_MANYLINUX1 -std=gnu99 ${CFLAGS:-}"

export PATH=$HOME/bin:/usr/local/bin:$PATH

PKG_EXTENSION=rpm
