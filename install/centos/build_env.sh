#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Sets common environment variables requires for building PageSpeed stuff.

devtoolset_enable=/opt/rh/devtoolset-2/enable
if [ -f "$devtoolset_enable" ]; then
  # devtoolset_enable uses possibly unset vars, so disable set -u for it.
  set +u
  source "$devtoolset_enable"
  set -u
fi

export SSL_CERT_DIR=/etc/pki/tls/certs
export SSL_CERT_FILE=/etc/pki/tls/cert.pem

# TODO(cheesy): Is -std=c99 still required?
export CFLAGS="-DGPR_MANYLINUX1 -std=gnu99 ${CFLAGS:-}"

export PATH=$HOME/bin:/usr/local/bin:$PATH
