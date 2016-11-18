# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# System-specific make vars for Ubuntu.
#
# At the time of writing most of the Makefile defaults were actually for
# Ubuntu, so this file is somewhat redundant.

APACHE_CONTROL_PROGRAM=/etc/init.d/apache2
APACHE_LOG=/var/log/apache2/error.log
APACHE_MODULES=/usr/lib/apache2/modules
APACHE_CONF_FILE=/etc/apache2/apache2.conf
APACHE_PIDFILE=/var/run/apache2.pid
APACHE_PROGRAM=/usr/sbin/apache2
APACHE_ROOT=/etc/apache2
APACHE_STOP_COMMAND=stop
APACHE_USER=www-data
BINDIR=/usr/local/bin
SSL_CERT_DIR=/etc/ssl/certs
SSL_CERT_FILE_COMMAND=
