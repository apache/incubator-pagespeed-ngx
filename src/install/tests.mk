# This makefile includes various integration tests, and is meant to be usable
# in both development and deployment settings.
#
# Its interface is as follows:
# Exports:
#  apache_system_tests
# Imports:
#  apache_install_conf (should read OPT_REWRITE_TEST, OPT_PROXY_TEST,
#                       OPT_SLURP_TEST, OPT_SPELING_TEST, OPT_HTTPS_TEST,
#                       OPT_COVERAGE_TRACE_TEST, OPT_STRESS_TEST,
#                       OPT_SHARED_MEM_LOCK_TEST, OPT_GZIP_TEST,
#                       OPT_FURIOUS_TEST, OPT_XHEADER_TEST)
#  apache_debug_restart
#  apache_debug_stop
#  apache_debug_leak_test, apache_debug_proxy_test, apache_debug_slurp_test
#  APACHE_DEBUG_PORT
#  APACHE_HTTPS_PORT
#  APACHE_CTRL_BIN
#  APACHE_DEBUG_PAGESPEED_CONF
#  PAGESPEED_ROOT
#  INSTALL_DATA_DIR

# We want order of dependencies honored..
.NOTPARALLEL :

# Want |& support; and /bin/sh doesn't provide it at least on Ubuntu 11.04
SHELL=/bin/bash

# Make conf, log, and cache file locations accessible to apache_system_test.sh
export APACHE_DEBUG_PAGESPEED_CONF
export APACHE_LOG
export PAGESPEED_ROOT

apache_system_tests :
	$(MAKE) apache_debug_smoke_test
	$(MAKE) apache_debug_leak_test
	$(MAKE) apache_debug_rewrite_test
	$(MAKE) apache_debug_proxy_test
	$(MAKE) apache_debug_slurp_test
	$(MAKE) apache_debug_serf_empty_header_test
	$(MAKE) apache_debug_speling_test
	$(MAKE) apache_debug_gzip_test
	$(MAKE) apache_debug_furious_test
	$(MAKE) apache_debug_xheader_test
	$(MAKE) apache_debug_vhost_only_test
	$(MAKE) apache_debug_global_off_test
	$(MAKE) apache_debug_shared_mem_lock_sanity_test
	$(MAKE) apache_install_conf
# 'apache_install_conf' should always be last, to leave your debug
# Apache server in a consistent state.

WGET = wget --no-proxy
WGET_PROXY = http_proxy=$(APACHE_SERVER) wget -q -O -
ifeq ($(APACHE_DEBUG_PORT),80)
  APACHE_SERVER = localhost
else
  APACHE_SERVER = localhost:$(APACHE_DEBUG_PORT)
endif
ifeq ($(APACHE_HTTPS_PORT),)
  APACHE_HTTPS_SERVER =
else ifeq ($(APACHE_HTTPS_PORT),443)
  APACHE_HTTPS_SERVER = localhost
else
  APACHE_HTTPS_SERVER = localhost:$(APACHE_HTTPS_PORT)
endif
EXAMPLE = $(APACHE_SERVER)/mod_pagespeed_example
EXAMPLE_IMAGE = $(EXAMPLE)/images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg
EXAMPLE_BIG_CSS = $(EXAMPLE)/styles/big.css.pagespeed.ce.01O-NppLwe.css
EXAMPLE_COMBINE_CSS = $(EXAMPLE)/combine_css.html

# Installs debug configuration and runs a smoke test against it.
# This will blow away your existing pagespeed.conf,
# and clear the cache.  It will also run with statistics off at the end,
# restoring it at the end
apache_debug_smoke_test : apache_install_conf apache_debug_restart
	@echo '***' System-test with cold cache
	-$(APACHE_CTRL_BIN) stop
	sleep 2
	rm -rf $(PAGESPEED_ROOT)/cache
	$(APACHE_CTRL_BIN) start
	$(INSTALL_DATA_DIR)/system_test.sh $(APACHE_SERVER) \
	                                   $(APACHE_HTTPS_SERVER)
	$(INSTALL_DATA_DIR)/apache_system_test.sh $(APACHE_SERVER) \
	                                          $(APACHE_HTTPS_SERVER)
	@echo '***' System-test with warm cache
	$(INSTALL_DATA_DIR)/system_test.sh $(APACHE_SERVER) \
	                                   $(APACHE_HTTPS_SERVER)
	$(INSTALL_DATA_DIR)/apache_system_test.sh $(APACHE_SERVER) \
	                                          $(APACHE_HTTPS_SERVER)
	@echo '***' System-test With statistics off
	mv $(APACHE_DEBUG_PAGESPEED_CONF) $(APACHE_DEBUG_PAGESPEED_CONF).save
	sed -e "s/# ModPagespeedStatistics off/ModPagespeedStatistics off/" \
		< $(APACHE_DEBUG_PAGESPEED_CONF).save \
		> $(APACHE_DEBUG_PAGESPEED_CONF)
	grep ModPagespeedStatistics $(APACHE_DEBUG_PAGESPEED_CONF)
	-$(APACHE_CTRL_BIN) restart
	sleep 2
	$(INSTALL_DATA_DIR)/system_test.sh $(APACHE_SERVER) \
	                                   $(APACHE_HTTPS_SERVER)
	$(INSTALL_DATA_DIR)/apache_system_test.sh $(APACHE_SERVER) \
	                                          $(APACHE_HTTPS_SERVER)
	mv $(APACHE_DEBUG_PAGESPEED_CONF).save $(APACHE_DEBUG_PAGESPEED_CONF)
	grep ModPagespeedStatistics $(APACHE_DEBUG_PAGESPEED_CONF)
	$(MAKE) apache_debug_stop
	[ -z "`grep leaked_rewrite_drivers $(APACHE_LOG)`" ]

apache_debug_rewrite_test : rewrite_test_prepare apache_install_conf \
    apache_debug_restart
	sleep 2
	$(WGET) -q -O - --save-headers $(EXAMPLE_IMAGE) \
	  | head -13 | grep "Content-Type: image/jpeg"
	$(WGET) -q -O - $(APACHE_SERVER)/mod_pagespeed_statistics \
	  | grep cache_hits
	$(WGET) -q -O - $(APACHE_SERVER)/shortcut.html \
	  | grep "Filter Examples"

rewrite_test_prepare:
	$(eval OPT_REWRITE_TEST="REWRITE_TEST=1")
	rm -rf $(PAGESPEED_ROOT)/cache/*

# This test checks that when mod_speling is enabled we handle the
# resource requests properly by nulling out request->filename.  If
# we fail to do that then mod_speling rewrites the result to be a 300
# (multiple choices).
apache_debug_speling_test : speling_test_prepare apache_install_conf \
    apache_debug_restart
	@echo Testing compatibility with mod_speling:
	$(WGET) -O /dev/null --save-headers $(EXAMPLE_IMAGE) 2>&1 \
	  | head | grep "HTTP request sent, awaiting response... 200 OK"

speling_test_prepare:
	$(eval OPT_SPELING_TEST="SPELING_TEST=1")
	rm -rf $(PAGESPEED_ROOT)/cache/*

# This test checks that when ModPagespeedFetchWithGzip is enabled we
# fetch resources from origin with the gzip flag.  Note that big.css
# uncompressed is 4307 bytes.  As of Jan 2012 we get 339 bytes, but
# the compression is done by mod_deflate which might change.  So we
# do a cumbersome range-check that the 4307 bytes gets compressed to
# somewhere between 200 and 500 bytes.
apache_debug_gzip_test : gzip_test_prepare apache_install_conf \
    apache_debug_restart
	@echo Testing efficacy of ModPagespeedFetchWithGzip:
	$(WGET) -O /dev/null --save-headers $(EXAMPLE_BIG_CSS) 2>&1 \
	  | head | grep "HTTP request sent, awaiting response... 200 OK"
	bytes=`$(WGET) -q -O - $(APACHE_SERVER)/mod_pagespeed_statistics \
	  | grep 'serf_fetch_bytes_count: ' | cut -d\  -f2`; \
	  echo Compressed big.css took $$bytes bytes; \
	  test $$bytes -gt 200 -a $$bytes -lt 500

gzip_test_prepare:
	$(eval OPT_GZIP_TEST="GZIP_TEST=1")
	rm -rf $(PAGESPEED_ROOT)/cache/*

# Test to make sure Furious is sending its headers
# TODO(nforman): Make this run multiple times and make sure we don't *always*
# get the same result.
apache_debug_furious_test : furious_test_prepare apache_install_conf \
    apache_debug_restart
	@echo Testing whether or not Furious is working:
	$(WGET) -q -O - --save-headers $(EXAMPLE) \
	  | grep "_GFURIOUS="
	matches=`$(WGET) -q -O - --save-headers \
          '$(EXAMPLE)?ModPagespeed=on&ModPagespeedFilters=rewrite_css' \
	  | grep -c '_GFURIOUS='`; \
	test $$matches -eq 0

furious_test_prepare:
	$(eval OPT_FURIOUS_TEST="FURIOUS_TEST=1")
	rm -rf $(PAGESPEED_ROOT)/cache/*

# This test checks that the ModPagespeedXHeaderValue directive works.
apache_debug_xheader_test : xheader_test_prepare apache_install_conf \
    apache_debug_restart
	@echo Testing ModPagespeedXHeaderValue directive:
	$(WGET) -q -O - --save-headers $(EXAMPLE) \
	  | egrep "^X-Mod-Pagespeed:|^X-Page-Speed:"
	value=`$(WGET) -q -O - --save-headers '$(EXAMPLE)' \
	  | egrep "^X-Mod-Pagespeed:|^X-Page-Speed:" \
	  | sed -e 's/^X-Mod-Pagespeed: *//' \
	  | sed -e 's/^X-Page-Speed: *//' \
	  | tr -d '\r'`; \
	test "$$value" = "UNSPECIFIED VERSION"

xheader_test_prepare:
	$(eval OPT_XHEADER_TEST="XHEADER_TEST=1")
	rm -rf $(PAGESPEED_ROOT)/cache/*

# Test to make sure we don't crash if we're off for global but on for vhosts.
# We use the stress test config as a base for that, as it has the vhosts all
# setup nicely; we just need to turn off ourselves for the global scope.
apache_debug_vhost_only_test:
	$(MAKE) apache_install_conf \
	  OPT_COVERAGE_TRACE_TEST=COVERAGE_TRACE_TEST=1 \
	  OPT_STRESS_TEST=STRESS_TEST=1
	echo 'ModPagespeed off' >> $(APACHE_DEBUG_PAGESPEED_CONF)
	$(MAKE) apache_debug_restart
	$(WGET) -O /dev/null --save-headers $(EXAMPLE) 2>&1 \
	  | head | grep "HTTP request sent, awaiting response... 200 OK"

# Regression test for serf fetching something with an empty header.
# We use a slurp-serving server to produce that.
EMPTY_HEADER_URL=http://www.modpagespeed.com/empty_header.html
apache_debug_serf_empty_header_test:
	$(MAKE) apache_install_conf \
	  OPT_COVERAGE_TRACE_TEST=COVERAGE_TRACE_TEST=1 \
	  OPT_STRESS_TEST=STRESS_TEST=1 \
	  SLURP_DIR=$(PWD)/$(INSTALL_DATA_DIR)/mod_pagespeed_test/slurp
	$(MAKE) apache_debug_restart
	# Make sure we can fetch a URL with empty header correctly..
	$(WGET_PROXY) $(EMPTY_HEADER_URL) > /dev/null


# Test to make sure we don't crash due to uninitialized statistics if we
# are off by default but turned on in some place.
apache_debug_global_off_test:
	$(MAKE) apache_install_conf
	echo 'ModPagespeed off' >> $(APACHE_DEBUG_PAGESPEED_CONF)
	$(MAKE) apache_debug_restart
	$(WGET) -O /dev/null --save-headers $(EXAMPLE)?ModPagespeed=on 2>&1 \
	  | head | grep "HTTP request sent, awaiting response... 200 OK"

# Sanity-check that enabling shared-memory locks don't cause the
# system to crash, and a rewrite does successfully happen.
apache_debug_shared_mem_lock_sanity_test : shared_mem_lock_test_prepare \
    apache_install_conf apache_debug_restart
	$(WGET) -q -O /dev/null $(EXAMPLE_COMBINE_CSS)?ModPagespeedFilters=combine_css
	sleep 1
	$(WGET) -q -O - $(EXAMPLE_COMBINE_CSS)?ModPagespeedFilters=combine_css \
	 | grep "\.pagespeed\.cc\."

shared_mem_lock_test_prepare:
	$(eval OPT_SLURP_TEST="SHARED_MEM_LOCK_TEST=1")
	rm -rf $(PAGESPEED_ROOT)/cache/*
