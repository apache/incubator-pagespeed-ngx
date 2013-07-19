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

# Block 1: Define upstream server's host and port.
backend default {
  # Location of PageSpeed server.
  .host = "127.0.0.1";
  .port = "8080";
}

# Block 2: Define a key based on the User-Agent which can be used for hashing.
# Also set the PS-CapabilityList header for PageSpeed server to respect.
sub generate_user_agent_based_key {
    # Define placeholder PS-CapabilityList header values for large and small
    # screens with no UA dependent optimizations. Note that these placeholder
    # values should not contain any of ll, ii, dj, jw or ws, since these
    # codes will end up representing optimizations to be supported for the
    # request.
    set req.http.default_ps_capability_list_for_large_screens = "LargeScreen.SkipUADependentOptimizations:";
    set req.http.default_ps_capability_list_for_small_screens = "TinyScreen.SkipUADependentOptimizations:";

    # As a fallback, the PS-CapabilityList header that is sent to the upstream
    # PageSpeed server should be for a large screen device with no browser
    # specific optimizations.
    set req.http.PS-CapabilityList = req.http.default_ps_capability_list_for_large_screens;

    # Cache-fragment 1: Desktop User-Agents that support lazyload_images (ll),
    # inline_images (ii) and defer_javascript (dj).
    # Note: Wget is added for testing purposes only.
    if (req.http.User-Agent ~ "(?i)Chrome/|Firefox/|MSIE |Safari|Wget") {
      set req.http.PS-CapabilityList = "ll,ii,dj:";
    }
    # Cache-fragment 2: Desktop User-Agents that support lazyload_images (ll),
    # inline_images (ii), defer_javascript (dj), webp (jw) and lossless_webp
    # (ws).
    if (req.http.User-Agent ~
        "(?i)Chrome/[2][3-9]+\.|Chrome/[[3-9][0-9]+\.|Chrome/[0-9]{3,}\.") {
      set req.http.PS-CapabilityList = "ll,ii,dj,jw,ws:";
    }
    # Cache-fragment 3: This fragment contains (a) Desktop User-Agents that
    # should not map to fragments 1 or 2 and (b) all tablet User-Agents. These
    # will only get optimizations that work on all browsers and use image
    # compression qualities applicable to large screens. Note that even tablets
    # that are capable of supporting inline or webp images, for e.g. Android
    # 4.1.2, will not get these advanced optimizations.
    if (req.http.User-Agent ~ "(?i)Firefox/[1-2]\.|MSIE [5-8]\.|bot|Yahoo!|Ruby|RPT-HTTPClient|(Google \(\+https\:\/\/developers\.google\.com\/\+\/web\/snippet\/\))|Android|iPad|TouchPad|Silk-Accelerated|Kindle Fire") {
      set req.http.PS-CapabilityList = req.http.default_ps_capability_list_for_large_screens;
    }
    # Cache-fragment 4: Mobiles and small screen tablets will use image compression
    # qualities applicable to small screens, but all other optimizations will be
    # those that work on all browsers.
    if (req.http.User-Agent ~ "(?i)Mozilla.*Android.*Mobile*|iPhone|BlackBerry|Opera Mobi|Opera Mini|SymbianOS|UP.Browser|J-PHONE|Profile/MIDP|portalmmm|DoCoMo|Obigo|Galaxy Nexus|GT-I9300|GT-N7100|HTC One|Nexus [4|7|S]|Xoom|XT907") {
      set req.http.PS-CapabilityList = req.http.default_ps_capability_list_for_small_screens;
    }
    # Remove placeholder header values.
    remove req.http.default_ps_capability_list_for_large_screens;
    remove req.http.default_ps_capability_list_for_large_screens;
}

sub vcl_hash {
  # Block 3: Use the PS-CapabilityList value for computing the hash.
  hash_data(req.http.PS-CapabilityList);
}

# Block 3a: Define ACL for purge requests
acl purge {
  # Purge requests are only allowed from localhost.
  "localhost";
  "127.0.0.1";
}

# Block 3b: Issue purge when there is a cache hit for the purge request.
sub vcl_hit {
  if (req.request == "PURGE") {
    purge;
    error 200 "Purged.";
  }
}

# Block 3c: Issue a no-op purge when there is a cache miss for the purge
# request.
sub vcl_miss {
  if (req.request == "PURGE") {
     purge;
     error 200 "Purged.";
  }
}

# Block 4: In vcl_recv, on receiving a request, call the method responsible for
# generating the User-Agent based key for hashing into the cache.
sub vcl_recv {
  call generate_user_agent_based_key;
  # Block 3d: Verify the ACL for an incoming purge request and handle it.
  if (req.request == "PURGE") {
    if (!client.ip ~ purge) {
      error 405 "Not allowed.";
    }
    return (lookup);
  }
  # Blocks which decide whether cache should be bypassed or not go here.
  # Block 5a: Bypass the cache for .pagespeed. resource. PageSpeed has its own
  # cache for these, and these could bloat up the caching layer.
  if (req.url ~ "\.pagespeed\.([a-z]\.)?[a-z]{2}\.[^.]{10}\.[^.]+") {
    # Skip the cache for .pagespeed. resource.  PageSpeed has its own
    # cache for these, and these could bloat up the caching layer.
    return (pass);
  }
  # Block 5b: Only cache responses to clients that support gzip.  Most clients
  # do, and the cache holds much more if it stores gzipped responses.
  if (req.http.Accept-Encoding !~ "gzip") {
    return (pass);
  }
}

# Block 6: Mark HTML uncacheable by caches beyond our control.
sub vcl_fetch {
   if (beresp.http.Content-Type ~ "text/html") {
     # Hide the upstream cache control headers.
     remove beresp.http.ETag;
     remove beresp.http.Last-Modified;
     remove beresp.http.Cache-Control;
     # Add no-cache Cache-Control header for html.
     set beresp.http.Cache-Control = "no-cache, max-age=0";
   }
   return (deliver);
}

# Block 7: Add a header for identifying cache hits/misses.
sub vcl_deliver {
  set resp.http.PS-CapabilityList = req.http.PS-CapabilityList;
  if (obj.hits > 0) {
    set resp.http.X-Cache = "HIT";
  } else {
    set resp.http.X-Cache = "MISS";
  }
}
