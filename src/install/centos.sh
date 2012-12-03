#!/bin/sh

set -x
exec make \
    APACHE_CONTROL_PROGRAM=/etc/init.d/httpd \
    APACHE_DOC_ROOT=/var/www/html \
    APACHE_LOG=/var/log/httpd/error_log \
    APACHE_MODULES=/etc/httpd/modules \
    APACHE_PIDFILE=/var/run/httpd.pid
    APACHE_PROGRAM=/usr/sbin/httpd \
    APACHE_ROOT=/etc/httpd \
    APACHE_STOP_COMMAND=graceful \
    APACHE_USER=apache \
    BINDIR=/usr/local/bin \
    $*
