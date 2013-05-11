#!/bin/sh

echo make $*

exec make \
    APACHE_CONTROL_PROGRAM=/etc/init.d/apache2 \
    APACHE_LOG=/var/log/apache2/error.log \
    APACHE_MODULES=/usr/lib/apache2/modules \
    APACHE_PIDFILE=/var/run/apache2.pid \
    APACHE_PROGRAM=/usr/sbin/apache2 \
    APACHE_ROOT=/etc/apache2 \
    APACHE_STOP_COMMAND=stop \
    BINDIR=/usr/local/bin \
    SSL_CERT_DIR=/etc/ssl/certs \
    SSL_CERT_FILE_COMMAND= \
    $*
