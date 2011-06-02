#!/bin/sh
set -x

SUFFIX=
if [ `uname -m` == x86_64 ]; then
    SUFFIX=64
fi

exec make \
    APACHE_CONTROL_PROGRAM=`readlink -f opensuse-initd-wrapper.sh` \
    APACHE_ROOT=/etc/apache2 \
    APACHE_MODULES=/usr/lib$SUFFIX/apache2 \
    APACHE_USER=wwwrun \
    APACHE_DOC_ROOT=/srv/www/htdocs \
    APACHE_LOG=/var/log/apache2/error_log \
    $*
