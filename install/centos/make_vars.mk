# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# System-specific make vars for CentOS.

APACHE_CONTROL_PROGRAM=/etc/init.d/httpd
APACHE_LOG=/var/log/httpd/error_log
APACHE_MODULES=/etc/httpd/modules
APACHE_CONF_FILE=/etc/httpd/conf/httpd.conf
APACHE_PIDFILE=/var/run/httpd.pid
APACHE_PROGRAM=/usr/sbin/httpd
APACHE_ROOT=/etc/httpd
APACHE_STOP_COMMAND=stop
APACHE_USER=apache
BINDIR=/usr/local/bin
SSL_CERT_DIR=/etc/pki/tls/certs
SSL_CERT_FILE_COMMAND=ModPagespeedSslCertFile /etc/pki/tls/cert.pem
