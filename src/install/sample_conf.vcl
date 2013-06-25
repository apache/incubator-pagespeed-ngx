/*
  This is the minimal VCL configuration required for passing the Apache
  mod_pagespeed system tests. To install varnish and start the varnish
  server at the right port, do the following:
  1) sudo apt-get install varnish
  2) sudo vim /etc/default/varnish and put in the following lines at the
     bottom of the file:
  DAEMON_OPTS="-a :8020 \
             -T localhost:6082 \
             -f /etc/varnish/default.vcl \
             -S /etc/varnish/secret \
             -s file,/var/lib/varnish/$INSTANCE/varnish_storage.bin,1G"
  3) sudo cp /path/to/install/sample_conf.vcl /etc/varnish/default.vcl
  4) sudo service varnish restart
*/

backend default {
  # Location of Apache mod_pagespeed server.
  .host = "127.0.0.1";
  .port = "8080";
}

acl purge {
  # Purge requests are only allowed from localhost.
  "localhost";
  "127.0.0.1";
}

sub vcl_recv {
  if (req.request == "PURGE") {
    if (!client.ip ~ purge) {
      error 405 "Not allowed.";
    }
    return (lookup);
  }
}

sub vcl_hit {
  if (req.request == "PURGE") {
    purge;
    error 200 "Purged.";
  }
}

sub vcl_miss {
  if (req.request == "PURGE") {
     purge;
     error 200 "Purged.";
  }
}

sub vcl_fetch {
   # Cache everything for 30s.
   set beresp.ttl   = 30s;
   set beresp.grace = 30s;
   set beresp.http.X-Cacheable = "YES";
   return (deliver);
}
