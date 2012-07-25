# This makefile includes various integration tests, and is meant to be usable
# in both development and deployment settings.
#
# Its interface is as follows:
# Exports:
#  apache_system_tests
#  apache_vm_system_tests  (includes tests that can be run on VMs)
# Imports:
#  apache_install_conf (should read OPT_REWRITE_TEST, OPT_PROXY_TEST,
#                       OPT_SLURP_TEST, OPT_SPELING_TEST, OPT_MEMCACHED_TEST,
#                       OPT_HTTPS_TEST,
#                       OPT_COVERAGE_TRACE_TEST, OPT_STRESS_TEST,
#                       OPT_SHARED_MEM_LOCK_TEST, OPT_GZIP_TEST,
#                       OPT_FURIOUS_GA_TEST, OPT_FURIOUS_NO_GA_TEST,
#                       OPT_URL_ATTRIBUTES_TEST, OPT_XHEADER_TEST,
#                       OPT_DOMAIN_HYPERLINKS_TEST,
#                       OPT_DOMAIN_RESOURCE_TAGS_TEST, OPT_ALL_DIRECTIVES_TEST)
#  apache_debug_restart
#  apache_debug_stop
#  apache_debug_leak_test, apache_debug_proxy_test, apache_debug_slurp_test
#  APACHE_DEBUG_PORT
#  APACHE_HTTPS_PORT
#  APACHE_CTRL_BIN
#  APACHE_DEBUG_PAGESPEED_CONF
#  MOD_PAGESPEED_CACHE
#  INSTALL_DATA_DIR

# We want order of dependencies honored..
.NOTPARALLEL :

# Want |& support; and /bin/sh doesn't provide it at least on Ubuntu 11.04
SHELL=/bin/bash

# Make conf, log, and cache file locations accessible to apache_system_test.sh
export APACHE_DEBUG_PAGESPEED_CONF
export APACHE_LOG
export MOD_PAGESPEED_CACHE

# We are seeing this failure when running checkin tsts when this test is run
# as part of apache_vm_system_tests:
#   [Tue Jul 24 20:06:21 2012] [error] [mod_pagespeed 0.10.0.0-1708 @2716] Memory cache failed
#   [Tue Jul 24 20:06:21 2012] [error] [mod_pagespeed 0.10.0.0-1708 @2722] Memory cache failed
#   [Tue Jul 24 20:06:21 2012] [error] [mod_pagespeed 0.10.0.0-1708 @2723] Failed to attach memcached server localhost:6765 Invalid argument
#   [Tue Jul 24 20:06:21 2012] [error] [mod_pagespeed 0.10.0.0-1708 @2723] Memory cache failed
# Commenting out until this is resolved.
#
#	$(MAKE) apache_debug_memcached_test

apache_vm_system_tests :
	$(MAKE) apache_debug_smoke_test
	$(MAKE) apache_debug_leak_test
	$(MAKE) apache_debug_rewrite_test
	$(MAKE) apache_debug_proxy_test
	$(MAKE) apache_debug_slurp_test
	$(MAKE) apache_debug_speling_test
	$(MAKE) apache_debug_gzip_test
	$(MAKE) apache_debug_furious_test
	$(MAKE) apache_debug_url_attribute_test
	$(MAKE) apache_debug_xheader_test
	$(MAKE) apache_debug_rewrite_hyperlinks_test
	$(MAKE) apache_debug_client_domain_rewrite_test
	$(MAKE) apache_debug_rewrite_resource_tags_test
	$(MAKE) apache_debug_vhost_only_test
	$(MAKE) apache_debug_global_off_test
	$(MAKE) apache_debug_shared_mem_lock_sanity_test
	$(MAKE) apache_debug_all_directives_test
	$(MAKE) apache_install_conf
# 'apache_install_conf' should always be last, to leave your debug
# Apache server in a consistent state.

# apache_debug_serf_empty_header_test fails when testing on VMs for
# release builds.  This appears to be due to the complicated proxy
# setup.
# TODO(jmarantz): fix this.
apache_system_tests : apache_vm_system_tests
	$(MAKE) apache_debug_serf_empty_header_test
	$(MAKE) apache_install_conf
	$(MAKE) apache_debug_restart

APACHE_HOST = localhost
ifeq ($(APACHE_DEBUG_PORT),80)
  APACHE_SERVER = $(APACHE_HOST)
else
  APACHE_SERVER = $(APACHE_HOST):$(APACHE_DEBUG_PORT)
endif
APACHE_SECONDARY_SERVER = $(APACHE_HOST):$(APACHE_SECONDARY_PORT)

WGET = wget
WGET_PROXY = http_proxy=$(APACHE_SERVER) $(WGET) -q -O -
WGET_NO_PROXY = $(WGET) --no-proxy
export WGET

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
TEST_ROOT = $(APACHE_SERVER)/mod_pagespeed_test

# Installs debug configuration and runs a smoke test against it.
# This will blow away your existing pagespeed.conf,
# and clear the cache.  It will also run with statistics off at the end,
# restoring it at the end
apache_debug_smoke_test : apache_install_conf apache_debug_restart
	@echo '***' System-test with cold cache
	-$(APACHE_CTRL_BIN) stop
	sleep 2
	rm -rf $(MOD_PAGESPEED_CACHE)
	$(APACHE_CTRL_BIN) start
	sleep 2
	$(INSTALL_DATA_DIR)/apache_system_test.sh $(APACHE_SERVER) \
	                                          $(APACHE_HTTPS_SERVER)
	@echo '***' System-test with warm cache
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
	CACHE_FLUSH_TEST=on \
	APACHE_SECONDARY_PORT=$(APACHE_SECONDARY_PORT) \
	APACHE_DOC_ROOT=$(APACHE_DOC_ROOT) \
	    $(INSTALL_DATA_DIR)/apache_system_test.sh \
	    $(APACHE_SERVER) $(APACHE_HTTPS_SERVER)
	mv $(APACHE_DEBUG_PAGESPEED_CONF).save $(APACHE_DEBUG_PAGESPEED_CONF)
	grep ModPagespeedStatistics $(APACHE_DEBUG_PAGESPEED_CONF)
	$(MAKE) apache_debug_stop
	[ -z "`grep leaked_rewrite_drivers $(APACHE_LOG)`" ]

apache_debug_rewrite_test : rewrite_test_prepare apache_install_conf \
    apache_debug_restart
	sleep 2
	$(WGET_NO_PROXY) -q -O - --save-headers $(EXAMPLE_IMAGE) \
	  | head -13 | grep "Content-Type: image/jpeg"
	$(WGET_NO_PROXY) -q -O - $(APACHE_SECONDARY_SERVER)/mod_pagespeed_statistics \
	  | grep cache_hits
	$(WGET_NO_PROXY) -q -O - $(APACHE_SECONDARY_SERVER)/shortcut.html \
	  | grep "Filter Examples"

rewrite_test_prepare:
	$(eval OPT_REWRITE_TEST="REWRITE_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

# This test checks that when mod_speling is enabled we handle the
# resource requests properly by nulling out request->filename.  If
# we fail to do that then mod_speling rewrites the result to be a 300
# (multiple choices).
apache_debug_speling_test : speling_test_prepare apache_install_conf \
    apache_debug_restart
	@echo Testing compatibility with mod_speling:
	$(WGET_NO_PROXY) -O /dev/null --save-headers $(EXAMPLE_IMAGE) 2>&1 \
	  | head | grep "HTTP request sent, awaiting response... 200 OK"

speling_test_prepare:
	$(eval OPT_SPELING_TEST="SPELING_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

apache_debug_memcached_test : memcached_test_prepare apache_install_conf \
    apache_debug_restart
	$(INSTALL_DATA_DIR)/run_program_with_memcached.sh -multi \
            $(INSTALL_DATA_DIR)/apache_system_test.sh $(APACHE_SERVER) \
	                                        $(APACHE_HTTPS_SERVER) \; \
        $(INSTALL_DATA_DIR)/apache_system_test.sh $(APACHE_SERVER) \
	    $(APACHE_HTTPS_SERVER)
	$(MAKE) apache_debug_stop
	[ -z "`grep leaked_rewrite_drivers $(APACHE_LOG)`" ]

memcached_test_prepare:
	$(eval OPT_MEMCACHED_TEST="MEMCACHED_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

# This test checks that when ModPagespeedFetchWithGzip is enabled we
# fetch resources from origin with the gzip flag.  Note that big.css
# uncompressed is 4307 bytes.  As of Jan 2012 we get 339 bytes, but
# the compression is done by mod_deflate which might change.  So we
# do a cumbersome range-check that the 4307 bytes gets compressed to
# somewhere between 200 and 500 bytes.
apache_debug_gzip_test : gzip_test_prepare apache_install_conf \
    apache_debug_restart
	@echo Testing efficacy of ModPagespeedFetchWithGzip:
	$(WGET_NO_PROXY) -O /dev/null --save-headers $(EXAMPLE_BIG_CSS) 2>&1 \
	  | head | grep "HTTP request sent, awaiting response... 200 OK"
	bytes=`$(WGET_NO_PROXY) -q -O - $(APACHE_SERVER)/mod_pagespeed_statistics \
	  | sed -n 's/serf_fetch_bytes_count: *//p'`; \
	  echo Compressed big.css took $$bytes bytes; \
	  test $$bytes -gt 200 -a $$bytes -lt 500

gzip_test_prepare:
	$(eval OPT_GZIP_TEST="GZIP_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

# Test to make sure Furious is sending its headers
# TODO(nforman): Make this run multiple times and make sure we don't *always*
# get the same result.
apache_debug_furious_test :
	$(MAKE) apache_debug_furious_ga_test
	$(MAKE) apache_debug_furious_no_ga_test

apache_debug_furious_ga_test : furious_ga_test_prepare apache_install_conf \
    apache_debug_restart
	$(INSTALL_DATA_DIR)/apache_furious_ga_test.sh $(APACHE_SERVER)

apache_debug_furious_no_ga_test : furious_no_ga_test_prepare \
 apache_install_conf apache_debug_restart
	$(INSTALL_DATA_DIR)/apache_furious_no_ga_test.sh $(APACHE_SERVER)

furious_ga_test_prepare:
	$(eval OPT_FURIOUS_GA_TEST="FURIOUS_GA_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

furious_no_ga_test_prepare:
	$(eval OPT_FURIOUS_NO_GA_TEST="FURIOUS_NO_GA_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

# This test checks that the ModPagespeedXHeaderValue directive works.
apache_debug_xheader_test : xheader_test_prepare apache_install_conf \
    apache_debug_restart
	@echo Testing ModPagespeedXHeaderValue directive:
	$(WGET_NO_PROXY) -q -O - --save-headers $(EXAMPLE) \
	  | egrep "^X-Mod-Pagespeed:|^X-Page-Speed:"
	value=`$(WGET_NO_PROXY) -q -O - --save-headers '$(EXAMPLE)' \
	  | egrep "^X-Mod-Pagespeed:|^X-Page-Speed:" \
	  | sed -e 's/^X-Mod-Pagespeed: *//' \
	  | sed -e 's/^X-Page-Speed: *//' \
	  | tr -d '\r'`; \
	test "$$value" = "UNSPECIFIED VERSION"

xheader_test_prepare:
	$(eval OPT_XHEADER_TEST="XHEADER_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

rewrite_hyperlinks_test_prepare:
	$(eval OPT_DOMAIN_HYPERLINKS_TEST="DOMAIN_HYPERLINKS_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

# This test checks that the ModPagespeedDomainRewriteHyperlinks directive
# can turn on.  See mod_pagespeed_test/rewrite_domains.html: it has
# one <img> URL, one <form> URL, and one <a> url, all referencing
# src.example.com.  They should all be rewritten to dst.example.com.
apache_debug_rewrite_hyperlinks_test : rewrite_hyperlinks_test_prepare \
    apache_install_conf apache_debug_restart
	@echo Testing ModPagespeedRewriteHyperlinks on directive:
	matches=`$(WGET_NO_PROXY) -q -O - $(TEST_ROOT)/rewrite_domains.html \
	  | grep -c http://dst\.example\.com`; \
	test $$matches -eq 3

client_domain_rewrite_test_prepare:
	$(eval OPT_CLIENT_DOMAIN_REWRITE_TEST="CLIENT_DOMAIN_REWRITE_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

# This test checks that the ModPagespeedClientDomainRewrite directive
# can turn on.
apache_debug_client_domain_rewrite_test : client_domain_rewrite_test_prepare \
    apache_install_conf apache_debug_restart
	@echo Testing ModPagespeedClientDomainRewrite on directive:
	matches=`$(WGET_NO_PROXY) -q -O - $(TEST_ROOT)/rewrite_domains.html \
	  | grep -c pagespeed\.clientDomainRewriterInit`; \
	test $$matches -eq 1

# Test to make sure dynamically defined url-valued attributes are rewritten by
# rewrite_domains.  See mod_pagespeed_test/rewrite_domains.html: in addition to
# having one <img> URL, one <form> URL, and one <a> url it also has one <span
# src=...> URL, one <hr imgsrc=...> URL, and one <hr src=...> URL, all
# referencing src.example.com.  The first three should be rewritten because of
# hardcoded rules, the span.src and hr.imgsrc should be rewritten because of
# ModPagespeedUrlValuedAttribute directives, and the hr.src should be left
# unmodified.  The rewritten ones should all be rewritten to dst.example.com.
apache_debug_url_attribute_test : url_attribute_test_prepare \
    apache_install_conf apache_debug_restart
	$(INSTALL_DATA_DIR)/apache_url_valued_attribute_test.sh $(APACHE_SERVER)

url_attribute_test_prepare:
	$(eval OPT_URL_ATTRIBUTE_TEST="URL_ATTRIBUTE_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

rewrite_resource_tags_test_prepare:
	$(eval OPT_DOMAIN_RESOURCE_TAGS_TEST="DOMAIN_RESOURCE_TAGS_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

# This test checks that the ModPagespeedDomainRewriteHyperlinks directive
# can turn on.  See mod_pagespeed_test/rewrite_domains.html: it has
# one <img> URL, one <form> URL, and one <a> url, all referencing
# src.example.com.  Only the <img> url should be rewritten.
apache_debug_rewrite_resource_tags_test : rewrite_resource_tags_test_prepare \
    apache_install_conf apache_debug_restart
	@echo Testing ModPagespeedRewriteHyperlinks off directive:
	matches=`$(WGET_NO_PROXY) -q -O - $(TEST_ROOT)/rewrite_domains.html \
	  | grep -c http://dst\.example\.com`; \
	test $$matches -eq 1

# Test to make sure we don't crash if we're off for global but on for vhosts.
# We use the stress test config as a base for that, as it has the vhosts all
# setup nicely; we just need to turn off ourselves for the global scope.
apache_debug_vhost_only_test:
	$(MAKE) apache_install_conf \
	  OPT_COVERAGE_TRACE_TEST=COVERAGE_TRACE_TEST=1 \
	  OPT_STRESS_TEST=STRESS_TEST=1
	echo 'ModPagespeed off' >> $(APACHE_DEBUG_PAGESPEED_CONF)
	$(MAKE) apache_debug_restart
	$(WGET_NO_PROXY) -O /dev/null --save-headers $(EXAMPLE) 2>&1 \
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
	$(WGET_NO_PROXY) -O /dev/null --save-headers $(EXAMPLE)?ModPagespeed=on 2>&1 \
	  | head | grep "HTTP request sent, awaiting response... 200 OK"

# Sanity-check that enabling shared-memory locks don't cause the
# system to crash, and a rewrite does successfully happen.
apache_debug_shared_mem_lock_sanity_test : shared_mem_lock_test_prepare \
    apache_install_conf apache_debug_restart
	$(WGET_NO_PROXY) -q -O /dev/null \
	    $(EXAMPLE_COMBINE_CSS)?ModPagespeedFilters=combine_css
	sleep 1
	$(WGET_NO_PROXY) -q -O - \
	    $(EXAMPLE_COMBINE_CSS)?ModPagespeedFilters=combine_css \
	 | grep "\.pagespeed\.cc\."

shared_mem_lock_test_prepare:
	$(eval OPT_SLURP_TEST="SHARED_MEM_LOCK_TEST=1")
	rm -rf $(MOD_PAGESPEED_CACHE)/*

# Test that all directives are accepted by the options parser.
apache_debug_all_directives_test:
	$(MAKE) apache_install_conf \
	  OPT_ALL_DIRECTIVES_TEST="ALL_DIRECTIVES_TEST=1"
	$(MAKE) apache_debug_restart
