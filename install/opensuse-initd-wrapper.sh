#!/bin/sh
# Takes care of conflict in shell vars used by us and system scripts
unset APACHE_MODULES
/etc/init.d/apache2 $*
