#!/bin/sh

set -x
exec make \
    APACHE_CONTROL_PROGRAM=/etc/init.d/httpd \
    APACHE_USER=apache \
    APACHE_DOC_ROOT=/var/www/html \
    $*
