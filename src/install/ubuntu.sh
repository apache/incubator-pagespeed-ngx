#!/bin/sh

exec make \
    APACHE_ROOT=/etc/apache2 \
    APACHE_MODULES=/usr/lib/apache2/modules \
    APACHE_CONTROL_PROGRAM=/etc/init.d/apache2 \
    APACHE_LOG=/var/log/apache2/error.log \
    $*
