# This Makefile contains targets for building, testing, and debugging
# mod_pagespeed during develoment.  Useful targets:
#
# apache_debug
# apache_release
#  - Build a development (debug), or release (optimized), version of
#    mod_pagespeed.
#
# apache_debug_psol
#  - Build a development version of PSOL, as '.a' file suitable for
#    ngx_pagespeed to link against.
#
# apache_test
#  - Run all unit tests.  You can also run the tests in halves with:
#    - mod_pagespeed_test
#      - Run unit tests under test.gyp:mod_pagespeed_test
#    - pagespeed_automatic_test
#      - Run unit tests under test.gyp:pagespeed_automatic_test
#  - You can run a specific unit test by setting TEST=test_name.
#    For example:
#      TEST=SerfUrlAsyncFetcherTest.FetchOneURLWithGzip
#    You can also run a subset of tests, using wildcards:
#      TEST=SerfUrlAsyncFetcherTest.* make apache_test
#    What you give to TEST is passed to googletest with --gtest_filter, so for
#    more details see the doc there:
#      https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#running-a-subset-of-the-tests
#
# apache_release_test
#  - Run all unit test with a release (non-debug) build.
#
# apache_root_test
#  - Currently broken test that builds install tarball, installs it as the root
#    apache on port 80 (requires sudo password) and runs most of our system
#    tests to make sure filters are working and rewritten resources are served.
#
# (This file also pulls in the contents of Makefile.tests, so some useful
#  targets are defined there instead.  To run system tests, for example, you can
#  run apache_debug_smoke_test.)
#
# apache_install_conf
#  - Copy install/debug.conf.template to your development apache directory,
#    making substitutions for @@VARS@@ and, depending on how OPTIONS is set,
#    uncommenting some #TESTVAR lines.
#  - When running tests this target is run automatically, but you may need to
#    run it manually during debugging.
#
# apache_debug_install
# apache_release_install
#  - Install the debug/release version of mod_pagespeed into the build of apache
#    that build_development_apache.sh created.
#  - This is another command that you probably only need when debugging
#    manually.
#
# apache_debug_start
# apache_debug_stop
# apache_debug_restart
#  - Start, stop, and restart the development apache instance.  This also
#    includes a few other potentially useful maintenance tasks, like cleaning up
#    temp files and rotating logs.
#  - Unless you specify FAST_RESTART=1, apache_debug_restart runs
#    apache_debug_install to make sure you're running the module you built and
#    not an earlier version.
#
# apache_debug_slurp
#  - mod_pagespeed can be run in "slurp" mode, where it is configured as a
#    forward proxy that saves a copy of everything that runs through it.  Later
#    on the slurp can be served in read-only mode, to serve the cached copy of
#    whatever was visited.  This is intended for testing and debugging, and is
#    what the load test uses.
#
# apache_debug_leak_test
#  - Run the unit tests and system tests under valgrind, to check for memory
#    errors and leaks.
#
# update_gyp_manifest
#  - If you add or remove a .gyp or .gypi file, run this target so we know to
#    watch it for changes.
#
# submodule_update
#  - The mod_pagespeed git repo includes its dependencies as git submodules.
#    These dependencies are pinned to a specific git revision.  If someone has
#    upgraded one, you need to run submodule_update to apply those changes.
#
# doxygen
#  - Builds html documentation for PSOL.  It includes everything defined as
#    public in the .h files.  You might ask: how do I know what things are safe
#    to depend on as consumer of PSOL, and which might change?  This is a good
#    question with a bad answer: you can depend on anything, and anything might
#    change.  We don't offer ABI or even API compatibility across versions, even
#    across point releases.
#
# This Makefile also includes various test targets that really should be in
# Makefile.tests:
#  - apache_debug_proxy_test
#  - apache_debug_slurp_test
# TODO(jefftk): move these into Makefile.tests
#
# It also includes other targets that are not indended to be run directly, and
# are either here to support devel/ scripts or to support other targets.
# somewhat redundant with the .gyp files, but helps bridge the gap and automate
# the transition.
#
# You can use the CONF= parameter to change build settings or enable coverage
# testing for debug builds:
#
#    CONF=Debug
#     - default, shouldn't be needed
#    CONF=Coverage
#     - run the test coverage analysis scripts after tests complete
#    CONF=Release
#     - shouldn't be needed; use targets with "release" in their name instead.
#    CONF=OptDebug
#     - build optimized binaries with debug info (-O2 -g)

# We need gcc-mozilla/bin to pick up our special gcc 4.8 for old versions of
# ubuntu.  It's ok to have it in the path even if we're on a machine where that
# directory doesn't exist; bash will just move on.
# TODO(jefftk): source ubuntu/make_vars.mk here, then move this there.
# TODO(jefftk): get this Makefile working on other platforms (centos, ubuntu16)
PATH := /usr/lib/gcc-mozilla/bin:/usr/local/bin:/usr/bin:$(PATH)
export PATH

GIT_SRC = $(realpath ${PWD}/..)
DEVEL_DIR = $(GIT_SRC)/devel
INSTALL_DATA_DIR = $(GIT_SRC)/install

# Where binaries go.
GIT_RELEASE_BIN = $(GIT_SRC)/out/Release
GIT_DEBUG_BIN = $(GIT_SRC)/out/Debug
GIT_BIN = $(GIT_SRC)/out/$(BUILDTYPE)

# GYP_MANIFEST holds a cached copy of the gyp files in this client.  When we
# first create a new checkout we run update_gyp_manifest, and from then on we
# use the cached copy when we need to know whether to rerun hooks.
GYP_MANIFEST = $(GIT_SRC)/gyp_manifest.d
.PHONY : update_gyp_manifest
update_gyp_manifest :
	find $(GIT_SRC) -name '*.gyp' -o -name '*.gypi' | \
	sort > $(GYP_MANIFEST)

# It's ok for this variable to be empty, and it will be empty the first time we
# run in a new checkout.  When it's empty we won't know whether to run gyp
# but on an initial checkout we run them regardless because we won't yet
# have the hook stamp.
GYP_FILES = $(shell [ -f $(GYP_MANIFEST) ] && cat $(GYP_MANIFEST) || echo "")

HOOKS_STAMP = $(GIT_SRC)/hooks.timestamp

ifeq ($(OBJDIR),)
OBJDIR := $(shell mktemp -td instaweb.XXXXXX)
endif
export OBJDIR
MAKE := $(MAKE) --no-print-directory OBJDIR=$(OBJDIR)

# Unset environment variables that can interfere with the make process.
# TODO(vchudnov): Not all of these may be problematic. Narrow down
# this list if possible.
unexport CC CXX GYP_DEFINES GYP_GENERATORS http_proxy

# gcc-mozilla contains /usr/lib/gcc-mozilla/bin/gcc but not
# /usr/lib/gcc-mozilla/bin/cc so if we want to use gcc-mozilla, which we do on
# Ubuntu 12 LTS, then we need to explicitly set CC=gcc.
# TODO(jefftk): do this in a way that lets people explicitly choose to build
# with clang instead of forcing gcc on everyone.
export CC=gcc

# By default, precompiled JS files are used. When developing, though, we want to
# enable local compilation of JS, so export the flag that enables use of the
# closure compiler. Note that this var is checked when gyp is run, not at
# compile time.
export BUILD_JS=1

# Run the proxy tests from a host configured via
#	cd install; ./ubuntu.sh setup_test_machine
# We use 'export' here so that apache/system_test.sh can see it.

PAGESPEED_TEST_HOST ?= selfsigned.modpagespeed.com
export PAGESPEED_TEST_HOST

# We don't want our targets to be run parallelized, as we want
# various directory maintenance steps to be run in-order
# This prevents that from happening, but still passes -j to actual
# compilation
.NOTPARALLEL :

.PHONY : prepare_objdir
prepare_objdir : $(OBJDIR)
CHECKIN_PREP_TARGETS = test_no_var_growth prepare_objdir exes

.PHONY : checkin_prep echo_checkin_prep
checkin_prep : $(CHECKIN_PREP_TARGETS)
echo_checkin_prep :
	@echo $(CHECKIN_PREP_TARGETS)

# Determine gyp BUILDTYPE and hooks we should run based on the passed in CONF
# TOOD(jefftk): The difference between setting CONF, BUILDTYPE, and running a
# target that sets BUILDTYPE makes it pretty hard to know what BUILDTYPE(s) a
# section of this Makefile might run under.  Clean this up.
CONF ?= Debug
HOOKTYPE = NoHooks
BUILDTYPE = $(CONF)

ifeq ($(CONF),Coverage)
  override BUILDTYPE=Debug_Coverage
  override HOOKTYPE=Coverage
else ifeq ($(CONF),OptDebug)
  # For -O2 -g type builds, we have to build as BUILDTYPE=Release CXXLAGS=-g;
  # with BUILDTYPE=Debug CXXFLAGS="-O2" we would get a -O2 -g -O0 out of gyp,
  # with the -O0 cancelling the -O2.
  override BUILDTYPE=Release
  # We need to make sure to add -g only once (since otherwise recursive
  # invocation will end up with something like -g -g -g and force a rebuild)
  ifeq (,$(findstring -g,$(CFLAGS)))
    override CFLAGS+= -g
    override CXXFLAGS+= -g
  endif
  export CFLAGS
endif

# We'd like to put this in gyp file, but that makes it hard to compile
# on old versions of g++ that do not support this flag.  However we
# can add the flag during development.  Don't add this in if it's
# already present; that causes cxxflags to mismatch adding extra compiles.
ifeq (,$(findstring -Wtype-limits,$(CXXFLAGS)))
  override CXXFLAGS+= -Wtype-limits
endif
export CXXFLAGS
export BUILDTYPE

# test to make sure that CXXFLAGS doesn't grow on each recursion
.PHONY : test_no_var_growth
test_no_var_growth :
	[ "$(CXXFLAGS)" = \
	  "`$(MAKE) echo_var VAR=CXXFLAGS `" ] || \
	  $(DEVEL_DIR)/expectfail echo CXXFLAGS changed on Makefile recursion

APACHE_RELEASE_MODULES_DIR=/usr/lib/apache2/modules

APACHE_DEBUG_ROOT ?= $(HOME)/apache2

# Note: these must be distinct from the ports used for apache_root_test.
APACHE_PORT = 8080
APACHE_SLURP_ORIGIN_PORT = 8081
APACHE_SLURP_PORT = 8082
APACHE_SECONDARY_PORT = 8083
APACHE_TERTIARY_PORT = 8085
APACHE_HTTPS_PORT = 8443
CONTROLLER_PORT = 8086
RCPORT=9091
# If you add additional ports here, make sure they don't overlap with the test
# ports ngx_pagespeed uses.  Check ngx_pagespeed/test/run_tests.sh for the list.

OPTIONS = $(EXTRA_OPTIONS) HTTPS_TEST=1
APACHE_DEBUG_MODULES = $(APACHE_DEBUG_ROOT)/modules
APACHE_DEBUG_BIN = $(APACHE_DEBUG_ROOT)/bin
MOD_PAGESPEED_CACHE = $(APACHE_DEBUG_ROOT)/pagespeed_cache
MOD_PAGESPEED_LOG = $(APACHE_DEBUG_ROOT)/pagespeed_log
TMP_SLURP_DIR = /tmp/instaweb/$(USER)/slurp
# Not used in this Makefile, but needed for some submake/script.
APACHE_LOG = $(APACHE_DEBUG_ROOT)/logs/error_log
APACHE_DEBUG_PAGESPEED_CONF = $(APACHE_DEBUG_ROOT)/conf/pagespeed.conf
APACHE_DOC_ROOT = $(APACHE_DEBUG_ROOT)/htdocs

APACHE_SERVER = http://localhost:$(APACHE_PORT)

# We want to track borgingssl branch chromium-stable at head, but submodules
# tracks a specific commit ID.
# If we need more deps like this, we should figure out a more generic solution.
UPDATE_BORINGSSL = git checkout chromium-stable && git pull

# These hooks are invoked before/after apache or tests are run
.PHONY : pre_start_NoHooks post_run_NoHooks pre_start_Coverage post_run_Coverage
.PHONY : pre_start post_run
pre_start_NoHooks :
post_run_NoHooks :

pre_start_Coverage :
	$(DEVEL_DIR)/gcov-all.sh --prepare $(GIT_SRC)

post_run_Coverage :
	$(DEVEL_DIR)/gcov-all.sh --summarize $(GIT_SRC)

pre_start : pre_start_$(HOOKTYPE)

post_run : post_run_$(HOOKTYPE)

# We check closure-compiled javascript into git to enable people to use
# closure-compiled output without needing to have a closure compiler dependency
# in the build process.
GENFILES = $(GIT_SRC)/net/instaweb/genfiles

.PHONY : apache_debug apache_debug_psol

# Sets up a development tree, and builds the modules
apache_debug : $(HOOKS_STAMP)
	@echo "building Apache module $@ ..."
	cd $(GIT_SRC) && $(MAKE)
	@echo Built mod_pagespeed successfully:
	ls -l $(GIT_DEBUG_BIN)/libmod_pagespeed.so
	@echo To install, type
	@echo "   " cp $(GIT_DEBUG_BIN)/libmod_pagespeed.so \
		$(APACHE_DEBUG_ROOT)/modules/mod_pagespeed.so

apache_debug_psol : apache_debug
	cd $(GIT_SRC)/pagespeed/automatic && $(MAKE) \
		MOD_PAGESPEED_ROOT=$(GIT_SRC) \
		OUTPUT_DIR=$(GIT_DEBUG_BIN) \
		BUILDTYPE=Debug \
		CXXFLAGS=$(CXXFLAGS) \
		all
	@echo Built PSOL successfully under in $(GIT_DEBUG_BIN)

.PHONY : submodule_update
submodule_update:
	@echo "Updating local checkouts from open source. If these generate any"
	@echo "merge conflicts you'll need to resolve them manually."
	cd $(GIT_SRC) && git pull --ff-only && git submodule update --recursive

.PHONY : exes apache_debug_install apache_debug_stop apache_debug_start
exes : apache_debug_install
apache_debug_install : apache_debug
	install -c $(GIT_BIN)/libmod_pagespeed.so \
	           $(APACHE_DEBUG_MODULES)/mod_pagespeed.so
	install -c $(GIT_BIN)/libmod_pagespeed_ap24.so \
	           $(APACHE_DEBUG_MODULES)/mod_pagespeed_ap24.so

apache_debug_stop :
	$(MAKE) stop
	$(MAKE) post_run

apache_debug_start :
	$(MAKE) pre_start
	$(MAKE) stop
	$(DEVEL_DIR)/apache_cleanup.sh $(USER)
	-$(DEVEL_DIR)/apache_rotate_logs.sh $(APACHE_DEBUG_ROOT)/logs
	$(MAKE) start

.PHONY : apache_debug_restart
ifeq ($(FAST_RESTART),1)
apache_debug_restart :
	$(MAKE) apache_debug_stop
	$(MAKE) apache_debug_start
else
# Restarts with newest module
apache_debug_restart :
	$(MAKE) apache_debug_stop
	$(MAKE) apache_debug_install
	$(MAKE) apache_debug_start
endif

# Installs a slurping pagespeed.conf on localhost:8082 and runs a debug
# binary.
.PHONY : apache_debug_slurp
apache_debug_slurp : slurp_test_prepare apache_install_conf
	$(MAKE) apache_debug_restart
	@echo Set your browser proxy to `hostname`:$(APACHE_SLURP_PORT)

# Stop and start Apache as reliably as possible.
.PHONY : stop start
stop :
	$(INSTALL_DATA_DIR)/stop_apache.sh $(APACHE_DEBUG_BIN)/apachectl \
				      $(APACHE_DEBUG_ROOT)/logs/httpd.pid \
				      $(APACHE_DEBUG_BIN)/httpd \
				      graceful-stop \
				      $(APACHE_PORT)

start :
	$(APACHE_DEBUG_BIN)/apachectl start
	@if [ ! -f $(APACHE_DEBUG_ROOT)/logs/httpd.pid ]; then \
	  /bin/echo -n "Waiting for httpd to start"; \
	  while [ ! -f $(APACHE_DEBUG_ROOT)/logs/httpd.pid ]; do \
	    /bin/echo -n "."; \
	    sleep 1; \
	  done; \
	  /bin/echo; \
	fi

.PHONY : apache_debug_leak_test
apache_debug_leak_test :
	rm -rf $(MOD_PAGESPEED_CACHE)/*  # Start with an empty cache
	$(DEVEL_DIR)/check_for_leaks \
	  $(APACHE_DEBUG_ROOT) localhost:$(APACHE_PORT)

include $(INSTALL_DATA_DIR)/Makefile.tests

# Installs the apache debug server and runs a stress test against it.
# This will blow away your existing cache and pagespeed.conf.
#
# Note: this test is obsolete; you probably want mps_load_test.sh or one of the
# siege tests.
#
# TODO(sligocki): Lock Apache.
apache_debug_stress_test : stress_test_prepare apache_install_conf
	$(MAKE) apache_debug_restart
	$(INSTALL_DATA_DIR)/stress_test.sh localhost:$(APACHE_PORT)
	$(MAKE) apache_debug_stop

stress_test_prepare :
	# stop old apaches, w/o generating any coverage reports,
	# (as we're about to do some new test)
	$(MAKE) stop
	$(eval OPTIONS+=STRESS_TEST=1)
	-rm -rf $(MOD_PAGESPEED_CACHE)/*

# This test checks that when ProxyPass is set on a host with mod_pagespeed
# enabled, URLs are rewritten correctly. (See install/debug.conf.template)
#   See: http://github.com/pagespeed/mod_pagespeed/issues/74
#
# TODO(sligocki): Lock Apache.
.PHONY : apache_debug_proxy_test proxy_test_prepare
apache_debug_proxy_test : proxy_test_prepare apache_install_conf
	$(MAKE) apache_debug_restart
	$(WGET_NO_PROXY) -q -O /dev/null \
	 $(APACHE_SERVER)/proxy_pass.html?PageSpeedFilters=extend_cache
	sleep 1
	$(WGET_NO_PROXY) -q -O - \
	 $(APACHE_SERVER)/proxy_pass.html?PageSpeedFilters=extend_cache\
	 | grep "localhost:8080/proxy_pass.css.pagespeed.ce"
	# Before this was fixed, it would be rewritten as localhost:8081/...

proxy_test_prepare :
	$(eval OPTIONS+=PROXY_TEST=1)

# This test checks that we can serve slurped pages, both from requests
# like those sent from a browser-proxy.  We'd also like to test
# requests sent from a fake DNS, like webpagetest, but I haven't
# figured out how to do that yet.  For now we'll just test read-only
# proxying.
#
# In our test flow, we first warm up mod_pagespeed's cache with a
# fetch and a sleep.  Then we test to make sure that we rewrite a
# resource properly, with mps on, and that we don't rewrite it with
# mps off.
#
# TODO(sligocki): Lock Apache.
.PHONY : apache_debug_slurp_test slurp_test_prepare
apache_debug_slurp_test : slurp_test_prepare apache_install_conf
	$(MAKE) apache_debug_restart
	$(DEVEL_DIR)/slurp_test.sh $(APACHE_SERVER) \
	  $(APACHE_SLURP_ORIGIN_PORT) $(APACHE_SLURP_PORT) $(WGET) \
	  $(TMP_SLURP_DIR) $(PAGESPEED_TEST_HOST)

slurp_test_prepare :
	$(eval OPTIONS+=SLURP_TEST=1)
	rm -rf $(MOD_PAGESPEED_CACHE)/*

# This is for trace-based stress tests, that operate on recorded URLs
# and slurp databases
# Note: you can pass PAR=, EXP= and RUNS= to stress test targets that
# execute the tests
.PHONY : check_dump_dir apache_trace_stress_test_prepare
check_dump_dir :
	$(if $(DUMP_DIR),,$(error "Need data directory set with DUMP_DIR="))
	$(if $(wildcard $(DUMP_DIR)/slurp),,\
	  $(error "No slurp/ under $(DUMP_DIR)"))

# mod_pagespeed load tests can be started with scripts/mps_load_test.sh.  If
# given an arg 'memcached', then that script will export MEMCACHED=1 and that
# will be passed to target 'apache_install_conf' as indicated below.  Same
# with cache_invalidation and inline_unauthorized_resources (IUR).
apache_trace_stress_test_prepare : check_dump_dir
	$(MAKE) stress_test_prepare
	echo MEMCACHED=$(MEMCACHED)
	echo REDIS=$(REDIS)
	echo PURGING=$(PURGING)
	echo IUR=$(IUR)
	echo IPRO_PRESERVE=$(IPRO_PRESERVE)
	$(MAKE) apache_install_conf SLURP_DIR=$(DUMP_DIR)/slurp SLURP_WRITE=0 \
		OPTIONS="$(OPTIONS) \
		LOADTEST_TEST=1 \
		STRESS_TEST=1 \
		HTTPS_TEST=0 \
		IPRO_PRESERVE_LOADTEST_TEST=$(IPRO_PRESERVE) \
		MEMCACHED_LOADTEST_TEST=$(MEMCACHED) \
		REDIS_LOADTEST_TEST=$(REDIS) \
		PURGING_LOADTEST_TEST=$(PURGING) \
		IUR_LOADTEST_TEST=$(IUR)"

apache_trace_stress_test_server :
	$(MAKE) apache_trace_stress_test_prepare
	$(MAKE) apache_debug_install
	$(MAKE) apache_debug_start

ifeq ($(TEST),)
TEST_ARG=
else
TEST_ARG=--gtest_filter=$(TEST)
endif

.PHONY : apache_test mod_pagespeed_test
apache_test : apache_debug
	$(MAKE) pre_start
	cd $(GIT_SRC) && \
		install/run_program_with_ext_caches.sh \
		$(GIT_BIN)/mod_pagespeed_test $(TEST_ARG)
	cd $(GIT_SRC) && \
		install/run_program_with_ext_caches.sh \
		$(GIT_BIN)/pagespeed_automatic_test $(TEST_ARG)
	$(MAKE) post_run

# TODO(jefftk): figure out if dividing the unit tests into mod_pagespeed_test
# and pagespeed_automatic_test is actually helpful.  Currently it looks like if
# there was once a principled distinction between the targets it has atrophied.
mod_pagespeed_test : apache_debug
	$(MAKE) pre_start
	cd $(GIT_SRC) && \
		install/run_program_with_ext_caches.sh \
		$(GIT_BIN)/mod_pagespeed_test $(TEST_ARG)
	$(MAKE) post_run

.PHONY : pagespeed_automatic_smoke_test
# TODO(jefftk): get rid of this and just use apache_debug_psol
pagespeed_automatic_smoke_test :
	@echo Building Pagespeed Automatic ...
	cd $(GIT_SRC)/pagespeed/automatic/ && \
		$(MAKE) MOD_PAGESPEED_ROOT=$(GIT_SRC) \
		OUTPUT_DIR=$(GIT_BIN) \
		CXXFLAGS=$(CXXFLAGS) \
		all

.PHONY : pagespeed_automatic_test
pagespeed_automatic_test : apache_debug
	$(MAKE) pre_start
	cd $(GIT_SRC) && \
		install/run_program_with_ext_caches.sh \
		$(GIT_BIN)/pagespeed_automatic_test $(TEST_ARG)
	$(MAKE) post_run

.PHONY : apache_release_test
apache_release_test : apache_release internal_release_test

TMP_PREFIX = /tmp/mod_pagespeed.$(USER).install

# Builds a binary release tarball, installs it as root (requires sudo password)
# and tests that rewrites occur.
#
# TODO(jmarantz): We are still leaving a bunch of stuff behind in /tmp; organize
# the temp dirs better and clean up.
.PHONY : apache_root_test internal_release_test

# TODO(jmarantz): This target is not working yet...it fails copying over
# pagespeed_libraries.conf.  Investigate:
#     cat common/pagespeed.load.template | \
#	    sed s~@@APACHE_MODULEDIR@@~/usr/lib/apache2/modules~ | \
#     sed s/@@COMMENT_OUT_DEFLATE@@// > /tmp/mod_pagespeed.install/pagespeed.load
#     cp -f /tmp/instaweb.vRV047/mod_pagespeed-test-jmarantz/install/net/instaweb/genfiles/conf/pagespeed_libraries.conf /tmp/mod_pagespeed.install/pagespeed_libraries.conf
#     cp: cannot stat ‘/tmp/instaweb.vRV047/mod_pagespeed-test-jmarantz/install/net/instaweb/genfiles/conf/pagespeed_libraries.conf’: No such file or directory
# TODO(jefftk): this also depends on prepare_release.sh and install-glucid.sh,
# that haven't been open sourced yet.
#apache_root_test :
#	sudo /etc/init.d/apache2 stop
#	sudo rm -rf $(TMP_PREFIX) /tmp/mod_pagespeed.install
#	cd ../.. && devel/prepare_release.sh -force \
#		 mod_pagespeed-test-$(USER)
#	cd $(OBJDIR) && tar xzf \
#		$(TMP_PREFIX)/mod_pagespeed-test-$(USER).tgz
#	cd $(OBJDIR)/mod_pagespeed-test-$(USER) && ./install-glucid.sh
#	rm -rf $(OBJDIR) /tmp/mod_pagespeed-test-$(USER).tgz
#	sudo rm -rf $(TMP_PREFIX) /tmp/mod_pagespeed.install

internal_release_test :
	cd $(GIT_SRC) && \
	  BUILDTYPE=Release install/run_program_with_ext_caches.sh \
	    $(GIT_RELEASE_BIN)/mod_pagespeed_test  $(TEST_ARG) && \
	  BUILDTYPE=Release install/run_program_with_ext_caches.sh \
	    $(GIT_RELEASE_BIN)/pagespeed_automatic_test $(TEST_ARG)

# Configuration root for Apache file-system and cache directories, to
# be written into config file.
#
# Installs a development copy of the the Instaweb server into $APACHE_DEBUG_ROOT
.PHONY : apache_install_conf
apache_install_conf : setup_doc_root
	rm -f $(APACHE_DEBUG_ROOT)/conf/pagespeed.conf
	mkdir -p $(MOD_PAGESPEED_CACHE)
	mkdir -p $(MOD_PAGESPEED_LOG)
	cd $(INSTALL_DATA_DIR) && \
	    $(MAKE) $(APACHE_DEBUG_ROOT)/conf/pagespeed.conf \
		STAGING_DIR=$(APACHE_DEBUG_ROOT)/conf \
		APACHE_DOC_ROOT=$(APACHE_DEBUG_ROOT)/htdocs \
		APACHE_MODULES=$(APACHE_DEBUG_MODULES) \
		APACHE_DOMAIN=$(APACHE_SERVER) \
		CONTROLLER_PORT=$(CONTROLLER_PORT) \
		RCPORT=$(RCPORT) \
		APACHE_SECONDARY_PORT=$(APACHE_SECONDARY_PORT) \
		APACHE_TERTIARY_PORT=$(APACHE_TERTIARY_PORT) \
		APACHE_HTTPS_DOMAIN=$(APACHE_HTTPS_SERVER) \
		MOD_PAGESPEED_CACHE=$(MOD_PAGESPEED_CACHE) \
		MOD_PAGESPEED_LOG=$(MOD_PAGESPEED_LOG) \
		MOD_PAGESPEED_ROOT=$(GIT_SRC) \
		SLURP_DIR=$(SLURP_DIR) \
		SLURP_WRITE=$(SLURP_WRITE) \
		PAGESPEED_TEST_HOST=$(PAGESPEED_TEST_HOST) \
		TMP_SLURP_DIR=$(TMP_SLURP_DIR) \
		$(OPTIONS)
	$(DEVEL_DIR)/apache_configure_https_port.sh $(APACHE_DEBUG_ROOT) \
	                                            $(APACHE_HTTPS_PORT)
	$(DEVEL_DIR)/apache_create_server_certificate.sh $(APACHE_DEBUG_ROOT)
	$(DEVEL_DIR)/apache_configure_php5_from_etc_php5.sh $(APACHE_DEBUG_ROOT)
	$(DEVEL_DIR)/apache_install.sh \
		$(APACHE_DEBUG_ROOT) $(APACHE_PORT) $(GIT_SRC)

# Builds the release objects.  This should not be run on its own -- it
# should be run manually via 'make apache_release'.
.PHONY : internal_release_build
internal_release_build : $(GIT_SRC) $(HOOKS_STAMP)
	@echo "building Apache release module ..."
	cd $(GIT_SRC) && BUILDTYPE=Release $(MAKE)

DOXYGEN_TARBALL = $(OBJDIR)/psol_doc.tar.gz
# Generates a doxygen tarball in $(DOXYGEN_TARBALL) suitable for
# scp to modpagespeed.com.
.PHONY : doxygen
doxygen :
	$(DEVEL_DIR)/doxify_tree.sh $(DOXYGEN_TARBALL)

.PHONY : apache_release apache_release_install
apache_release : internal_release_build
	@echo Release apache module built.  Install it with
	@echo "   " cd $(GIT_SRC)/install
	@echo "   " make staging
	@echo "   " sudo make install
	@echo "or"
	@echo "   " sudo cp \
		$(GIT_RELEASE_BIN)/libmod_pagespeed.so \
		$(APACHE_RELEASE_MODULES_DIR)/mod_pagespeed.so

apache_release_install : apache_release
	@echo "Copy the module to your Apache2 modules directory."
	@echo "Then, restart your Apache server."
	install -c $(GIT_RELEASE_BIN)/libmod_pagespeed.so \
	           $(APACHE_RELEASE_MODULES_DIR)/mod_pagespeed.so
	install -c $(GIT_RELEASE_BIN)/libmod_pagespeed_ap24.so \
	           $(APACHE_RELEASE_MODULES_DIR)/mod_pagespeed_ap24.so
	/etc/init.d/apache2 stop
	/etc/init.d/apache2 start

# Using pipefail to ensure grep -v doesn't obscure a gyp failure.
$(HOOKS_STAMP) : $(GPERF) $(GYP_FILES)
	bash -o pipefail -c \
	  "cd $(GIT_SRC) && python build/gyp_chromium --depth=. | \
	     egrep -v '^(Generating )[a-zA-Z0-9_/.-]*(Makefile|mk)$$'"
	touch $(HOOKS_STAMP)

$(OBJDIR) : make_obj_dirs

.PHONY : make_obj_dirs clean
make_obj_dirs : $(GIT_SRC)
	@echo Setting up library directories...
	@mkdir -p $(GIT_INSTAWEB_LIB_PATH) \
		$(GENFILES) \
		$(OBJDIR)/apache/net/instaweb \
		$(OBJDIR)/apache/pagespeed/opt/ads \
		$(OBJDIR)/apache/pagespeed/kernel/base \
		$(OBJDIR)/apache/pagespeed/kernel/cache \
		$(OBJDIR)/apache/pagespeed/kernel/html \
		$(OBJDIR)/apache/pagespeed/kernel/http \
		$(OBJDIR)/apache/pagespeed/kernel/image \
		$(OBJDIR)/apache/pagespeed/kernel/image/testdata/gif \
		$(OBJDIR)/apache/pagespeed/kernel/image/testdata/jpeg \
		$(OBJDIR)/apache/pagespeed/kernel/image/testdata/png \
		$(OBJDIR)/apache/pagespeed/kernel/image/testdata/pngsuite \
		$(OBJDIR)/apache/pagespeed/kernel/image/testdata/pngsuite/gif \
		$(OBJDIR)/apache/pagespeed/kernel/image/testdata/resized \
		$(OBJDIR)/apache/pagespeed/kernel/image/testdata/webp \
		$(OBJDIR)/apache/pagespeed/kernel/js \
		$(OBJDIR)/apache/pagespeed/kernel/sharedmem \
		$(OBJDIR)/apache/pagespeed/kernel/thread \
		$(OBJDIR)/apache/pagespeed/kernel/util \
		$(OBJDIR)/apache/pagespeed/install \
		$(OBJDIR)/apache/pagespeed/apache \
		$(OBJDIR)/apache/pagespeed/automatic \
		$(OBJDIR)/apache/pagespeed/automatic/public \
		$(OBJDIR)/apache/pagespeed/controller \
		$(OBJDIR)/apache/pagespeed/system \
		$(OBJDIR)/apache/pagespeed/system/public \
		$(OBJDIR)/css_parser
	@cd $(OBJDIR)/apache/net/instaweb; mkdir -p \
		config htmlparse/public http/public http/testdata \
		js/public public rewriter/public rewriter/testdata spriter \
		spriter/public util/public

clean :
	rm -rf $(OBJDIR)
	rm -rf $(GIT_SRC)/out

# Install gperf if it doesn't exist
GPERF = /usr/bin/gperf
$(GPERF) :
	sudo apt-get install gperf

.PHONY : echo_var echo_vars
echo_var :
	@echo $($(VAR))

echo_vars :
	@echo OBJDIR=$(OBJDIR)
	@echo G3_ROOT=$(G3_ROOT)
	@echo SANDBOX=$(SANDBOX)
	@echo GOOGLE_PROTO_FILES=$(GOOGLE_PROTO_FILES)
	@echo GIT_SRC=$(GIT_SRC)
	@echo GYP_MANIFEST=$(GYP_MANIFEST)
