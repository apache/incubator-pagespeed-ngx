#!/bin/bash
#
# Script to create a server certificate (and key) file for Apache,
# usually in ~/apache2.
#
# usage: apache_create_server_certificate.sh apache-root-directory

APACHE_ROOT=$1

# Create a cert file iff we don't already have one.
if [ ! -e $APACHE_ROOT/conf/server.crt ]; then
  openssl req -new -x509 -days 36500 -sha1 -newkey rsa:1024 -nodes \
    -keyout $APACHE_ROOT/conf/server.key \
    -out    $APACHE_ROOT/conf/server.crt \
    -subj "/O=Company/OU=Department/CN=$(hostname)"
  echo Certificate files were created: $APACHE_ROOT/conf/server.{key,crt}
fi

exit 0
