#!/bin/sh

echo make $*

APACHE_DOC_ROOT=/var/www
# Test for new ubuntu setups where doc/root is in /var/www/html.  We could run
# into a false positive here, but probably not for build systems.
if [ -e /var/www/html ]; then
  APACHE_DOC_ROOT=/var/www/html
fi

exec make \
    APACHE_CONTROL_PROGRAM=/etc/init.d/apache2 \
    APACHE_LOG=/var/log/apache2/error.log \
    APACHE_MODULES=/usr/lib/apache2/modules \
    APACHE_CONF_FILE=/etc/apache2/apache2.conf \
    APACHE_DOC_ROOT=$APACHE_DOC_ROOT \
    APACHE_PIDFILE=/var/run/apache2.pid \
    APACHE_PROGRAM=/usr/sbin/apache2 \
    APACHE_ROOT=/etc/apache2 \
    APACHE_STOP_COMMAND=stop \
    BINDIR=/usr/local/bin \
    SSL_CERT_DIR=/etc/ssl/certs \
    SSL_CERT_FILE_COMMAND= \
    $*
