#!/bin/sh

exec make \
    APACHE_ROOT=/etc/apache2 \
    APACHE_MODULES=/usr/lib/apache2/modules \
    $*
