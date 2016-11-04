# This Makefile is used to help drive the installation of mod_pagespeed into
# an Apache installation.
#
# Note that the location of the Apache configuration files may vary by
# Linux distribution.  For example, we have seen the following installation
# directories for the default Apache install.
#
#     Ubuntu 				/etc/apache2/mods-enabled/*.conf
#     CentOS 				/etc/httpd/conf.d/*.conf
#     Custom Apache build from source	/usr/local/apache2/conf/extra/
#
# In the case of the custom Apache build, you must also edit
# /usr/local/apache2/conf to add "Include conf/extra/pagespeed.conf"
#
# The goal of this Makefile is to help generate basic default
# configuration files that can then be edited to tune the HTML
# performance based on the Apache installation, internet-visible
# hostnames, and the specific needs of the site.
#
# The usage model of this Makefile is that, as an unpriviledged user, you
# create the desired configuration files in /tmp, where you can examine
# them before installing them.  You can then do either of these:
#
#    (a) Run "make -n install" to see the recommended installation commands,
#        and execute them by hand
#    (b) Run "sudo make install" to install them automatically.
#
#
# To install mod_pagespeed properly, we need to know the locations of
# Apache configuration scripts and binaries.  These can are specified
# as Makefile variables which can be overridden on the command line.
# They have defaults, which will often need to be changed.


# The location of the Apache root installation directory.  This helps form
# defaults for other variables, but each of those can be overridden.
APACHE_ROOT = /etc/httpd

# The installation directory for modules (mod*.so)
APACHE_MODULES        = $(APACHE_ROOT)/modules

# The root directory Apache uses for serving files.
ifeq "$(wildcard /var/www/html/.)" ""
  # /var/www/html doesn't exist, so use /var/www.
  APACHE_DOC_ROOT = /var/www
else
  APACHE_DOC_ROOT = /var/www/html
endif

# The domain Apache is serving from
#APACHE_DOMAIN        = localhost:8080  # For test-server.
APACHE_DOMAIN         = localhost
APACHE_HTTPS_DOMAIN   = localhost

# Define the ports used for the root install.  These must be distinct from
# the ones used for debug installs.
APACHE_PORT           = 80
APACHE_SECONDARY_PORT = 8087
APACHE_TERTIARY_PORT  = 8088
CONTROLLER_PORT       = 8090
RCPORT                = 8091

SSL_CERT_DIR          = /etc/ssl/certs
SSL_CERT_FILE_COMMAND =

# These are set via command-line when run via 'ubuntu.sh', 'centos.sh',
# or 'opensuse.sh'.  However during development we use this Makefile
# directly, so we set defaults from an Ubuntu version on our dev
# boxes (whose Apache lacks 'graceful-stop').
APACHE_CONTROL_PROGRAM   = /etc/init.d/httpd
APACHE_START             = $(APACHE_CONTROL_PROGRAM) start
APACHE_STOP_COMMAND      = stop
APACHE_PIDFILE           = /var/run/apache2.pid
APACHE_PROGRAM           = /usr/sbin/apache2

# For testing proxying of an external domain, this represents the domain we
# are proxying from.
PAGESPEED_TEST_HOST ?= modpagespeed.com
export PAGESPEED_TEST_HOST

# The installation directory for executables
BINDIR = /usr/bin

# A temp directory to stage generated configuration files.  This must be
# writable by the user, and readable by root.
STAGING_DIR = /tmp/mod_pagespeed.install

# The mod_pagespeed module is specified relative to the install directory,
# which is src/install.
MOD_PAGESPEED_ROOT = $(shell dirname `pwd`)
PAGESPEED_MODULE = $(MOD_PAGESPEED_ROOT)/out/Release/libmod_pagespeed.so
PAGESPEED_MODULE_24 = $(MOD_PAGESPEED_ROOT)/out/Release/libmod_pagespeed_ap24.so
PAGESPEED_JS_MINIFY = $(MOD_PAGESPEED_ROOT)/out/Release/js_minify

# On systems dervied from the NCSA configuration files by Rob McCool,
# you enable a module by writing its .conf file into
# $(APACHE_ROOT)/mods-available/pagespeed.conf, and a single Load command into
# $(APACHE_ROOT)/mods-enabled/pagespeed.conf.  So if that exists, then we'll
# try to automate that.
MODS_ENABLED_DIR = $(shell if [ -d $(APACHE_ROOT)/mods-enabled ]; then \
	echo $(APACHE_ROOT)/mods-enabled; fi)
MODS_AVAILABLE_DIR = $(shell if [ -d $(APACHE_ROOT)/mods-available ]; then \
	echo $(APACHE_ROOT)/mods-available; fi)

# Determines where mod_pagespeed should put cache.
MOD_PAGESPEED_CACHE = /var/cache/mod_pagespeed

# Determines where mod_pagespeed should write various logs.
MOD_PAGESPEED_LOG = /var/log/pagespeed

# The username used to run apache.  This is needed to create the directory
# used to store mod_pagespeed files and cache data.
APACHE_USER = www-data

# Set this to 1 to enable mod_proxy and mod_rewrite
ENABLE_PROXY = 0

.PHONY : config_file echo_vars

echo_vars :
	@echo Run "restart" to add default instaweb config to apache
	@echo Or run "stop", "staging", "install", and "start".
	@echo These configuration variables can be reset on the make command line,
	@echo e.g. \"make config_file\"
	@echo ""
	@echo "   APACHE_CONF_FILE=$(APACHE_CONF_FILE)"
	@echo "   APACHE_MODULES=$(APACHE_MODULES)"
	@echo "   APACHE_ROOT=$(APACHE_ROOT)"
	@echo "   APACHE_DOC_ROOT=$(APACHE_DOC_ROOT)"
	@echo "   APACHE_START=$(APACHE_START)"
	@echo "   APACHE_STOP_COMMAND=$(APACHE_STOP_COMMAND)"
	@echo "   MOD_PAGESPEED_CACHE=$(MOD_PAGESPEED_CACHE)"
	@echo "   MOD_PAGESPEED_LOG=$(MOD_PAGESPEED_LOG)"
	@echo "   MODS_ENABLED_DIR=$(MODS_ENABLED_DIR)"
	@echo "   MODS_AVAILABLE_DIR=$(MODS_AVAILABLE_DIR)"
	@echo "   STAGING_DIR=$(STAGING_DIR)"
	@echo "   ENABLE_PROXY=${ENABLE_PROXY}"
	@echo "   SLURP_DIR=${SLURP_DIR}"
	@echo "   SHARED_MEM_LOCKS=${SHARED_MEM_LOCKS}"


# In some Linux distributions, such as Ubuntu, there are two commands
# in the default root config file:
#    Include /etc/apache2/modes-enabled/*.load
#    Include /etc/apache2/modes-enabled/*.conf
# we need to write a one-line '.load' file and put that and our '.conf' file
# into .../mods-enabled.
#
# In other distributions, such as CentOS, there is an 'Include DIR/*.conf',
# but there is no implicit loading of modules, so we write our Load line
# directly into our config file.

# In either case, independent configuration files go here (this directory
# is read by both distributions on startup after the modules load).
APACHE_CONF_D = $(APACHE_ROOT)/conf.d

ifeq ($(MODS_ENABLED_DIR),)

# This is a CentOS-like installation, where there is no explicit .load
# file, and we instead pre-pend the LoadModule command to the .conf file.
APACHE_CONF_DIR = $(APACHE_CONF_D)
CONF_SOURCES = $(STAGING_DIR)/pagespeed.load $(STAGING_DIR)/pagespeed.conf

else

# This is an Ubuntu-like installation, where the .load files are placed
# separately into a mods-enabled directory, and the .conf file is loaded
# independently.
MODS_ENABLED_INSTALL_COMMANDS = \
	rm -f $(MODS_ENABLED_DIR)/pagespeed.load ; \
	cp -f $(STAGING_DIR)/pagespeed.load $(MODS_AVAILABLE_DIR) ; \
	cd $(MODS_ENABLED_DIR) && ln -s ../mods-available/pagespeed.load ; \
	rm -f $(MODS_ENABLED_DIR)/headers.load ; \
	cd $(MODS_ENABLED_DIR) && ln -s ../mods-available/headers.load ; \
	rm -f $(MODS_ENABLED_DIR)/deflate.load ; \
	cd $(MODS_ENABLED_DIR) && ln -s ../mods-available/deflate.load

APACHE_CONF_DIR = $(MODS_AVAILABLE_DIR)
CONF_SOURCES = $(STAGING_DIR)/pagespeed.conf

endif


# We will generate 'proxy.conf' in the staging area
# unconditiontionally, but we will load it into the
# Apache server only if the user installs with ENABLE_PROXY=1
ifeq ($(ENABLE_PROXY),1)
CONF_SOURCES += $(STAGING_DIR)/proxy.conf
endif

APACHE_SLURP_READ_ONLY_COMMAND=\#ModPagespeedSlurpReadOnly on

ifeq ($(SLURP_DIR),)
  APACHE_SLURP_DIR_COMMAND = \#ModPagespeedSlurpDirectory ...
else
  APACHE_SLURP_DIR_COMMAND = ModPagespeedSlurpDirectory $(SLURP_DIR)
  ifeq ($(SLURP_WRITE),1)
    APACHE_SLURP_READ_ONLY_COMMAND=ModPagespeedSlurpReadOnly off
  else
    APACHE_SLURP_READ_ONLY_COMMAND=ModPagespeedSlurpReadOnly on
  endif
endif

TESTS=REDIS MEMCACHED STRESS REWRITE LOADTEST PROXY SLURP SHARED_MEM_LOCKS \
      IPRO_PRESERVE_LOADTEST MEMCACHED_LOADTEST REDIS_LOADTEST \
      PURGING_LOADTEST SPELING GZIP EXPERIMENT_GA EXPERIMENT_NO_GA HTTPS \
      ALL_DIRECTIVES PER_VHOST_STATS NO_PER_VHOST_STATS STATS_LOGGING EXTCACHE \
      PROCESS_SCOPE VALGRIND REMOTE_CONFIG CONTROLLER

ifeq ($(REDIS_TEST),1)
  EXTCACHE_TEST=1
else ifeq ($(MEMCACHED_TEST),1)
  EXTCACHE_TEST=1
endif

# Compute the lists of tests that we will retain, by removing the "#TEST"
# substring, and will strip, by removing the entire line on which the #TEST
# appears.
CONFIG_RETAIN = $(foreach T, $(TESTS), $(if $(filter 1, $($T_TEST)), $T))
CONFIG_STRIP  = $(foreach T, $(TESTS), $(if $(filter 1, $($T_TEST)),,$T))

# Special logic for testing of inlining unauthorized resources, where the
# sed substitutions are not exactly aligned with the control-variables.
ifeq ($(IUR_LOADTEST_TEST),1)
  CONFIG_RETAIN += IUR_LOADTEST
  # remove whole explicit domain authorization line
  CONFIG_STRIP += DOMAIN_AUTH_LOADTEST
else
  CONFIG_STRIP += IUR_LOADTEST
  ifeq ($(LOADTEST_TEST),1)
    # remove prefix for explicit domain authorization line
    CONFIG_RETAIN += DOMAIN_AUTH_LOADTEST
  else
    CONFIG_STRIP += DOMAIN_AUTH_LOADTEST
  endif
endif

# Compute the patterns to pass to 'sed' for enabling lines by stripping out the
# comments, and to 'egrep -v' for stripping out entire lines.
SED_PATTERNS = $(foreach T, $(CONFIG_RETAIN),-e "s@^\#$T\b@@")
EGREP_V_PATTERN = \
    ($(shell echo "$(strip $(foreach T,$(CONFIG_STRIP),^\#$T\b))" | \
    tr '[:blank:]' '|'))

# Note that the quoted sed replacement for APACHE_SLURP_DIR_COMMAND is because
# that might have embedded spaces, and 'sed' is interpreted first by bash.

$(STAGING_DIR)/pagespeed.conf : common/pagespeed.conf.template debug.conf.template
	sed -e "s!@@APACHE_DOC_ROOT@@!$(APACHE_DOC_ROOT)!g" \
	    -e "s!@@APACHE_DOMAIN@@!$(APACHE_DOMAIN)!g" \
	    -e "s!@@APACHE_HTTPS_DOMAIN@@!$(APACHE_HTTPS_DOMAIN)!g" \
	    -e "s!@@APACHE_MODULES@@!$(APACHE_MODULES)!g" \
	    -e "s!@@APACHE_SECONDARY_PORT@@!$(APACHE_SECONDARY_PORT)!g" \
	    -e "s!@@APACHE_TERTIARY_PORT@@!$(APACHE_TERTIARY_PORT)!g" \
	    -e "s!@@CONTROLLER_PORT@@!$(CONTROLLER_PORT)!g" \
	    -e "s!@@PAGESPEED-TEST-HOST@@!$(PAGESPEED_TEST_HOST)!g" \
	    -e "s!@@MOD_PAGESPEED_CACHE@@!$(MOD_PAGESPEED_CACHE)!g" \
	    -e "s!@@MOD_PAGESPEED_LOG@@!$(MOD_PAGESPEED_LOG)!g" \
	    -e "s!@@RCPORT@@!$(RCPORT)!g" \
	    -e "s!@@SSL_CERT_DIR@@!$(SSL_CERT_DIR)!g" \
	    -e "s!@@SSL_CERT_FILE_COMMAND@@!$(SSL_CERT_FILE_COMMAND)!g" \
	    -e "s@# ModPagespeedSlurpDirectory ...@$(APACHE_SLURP_DIR_COMMAND)@g" \
	    -e "s@# ModPagespeedSlurpReadOnly on@$(APACHE_SLURP_READ_ONLY_COMMAND)@g" \
	    -e "s|@@TMP_SLURP_DIR@@|$(TMP_SLURP_DIR)|g" \
	    -e "s|@@MEMCACHED_PORT@@|$(MEMCACHED_PORT)|g" \
	    -e "s|@@REDIS_PORT@@|$(REDIS_PORT)|g" \
	    $(SED_PATTERNS) \
		$^ \
		| egrep -v "$(EGREP_V_PATTERN)" > $@
	! grep '@@' $@     # Make sure we don't have any remaining @@variables@@
	! grep '^#[^ ]' $@ # Make sure we don't have any remaining #variables

$(STAGING_DIR)/proxy.conf : proxy.conf.template
	sed -e s@APACHE_MODULES@$(APACHE_MODULES)@g \
		$< > $@

CONF_TEMPLATES = $(STAGING_DIR)/pagespeed.conf \
		 $(STAGING_DIR)/proxy.conf

setup_staging_dir :
	rm -rf $(STAGING_DIR)
	mkdir -p $(STAGING_DIR)

LIBRARY_CONF_SOURCE = \
    $(MOD_PAGESPEED_ROOT)/net/instaweb/genfiles/conf/pagespeed_libraries.conf

# Generate a configuration file and copy it to the staging area.
# Also copy the example tree, and the built Apache module
staging_except_module : setup_staging_dir $(CONF_TEMPLATES)
	cat common/pagespeed.load.template | \
	    sed s~@@APACHE_MODULEDIR@@~$(APACHE_MODULES)~ | \
	    sed s/@@COMMENT_OUT_DEFLATE@@// > $(STAGING_DIR)/pagespeed.load
	cp -f $(LIBRARY_CONF_SOURCE) $(STAGING_DIR)/pagespeed_libraries.conf
	$(MODS_ENABLED_STAGING_COMMANDS)
	cp -rp mod_pagespeed_example mod_pagespeed_test $(STAGING_DIR)
	@echo Changing security context of $(APACHE_DOC_ROOT)
	@echo This will fail and be ignored if your selinux isn\'t set up.
	-chcon -R --reference=$(APACHE_DOC_ROOT) $(STAGING_DIR)

staging : staging_except_module
	cp $(PAGESPEED_MODULE) $(STAGING_DIR)/mod_pagespeed.so
	cp $(PAGESPEED_MODULE_24) $(STAGING_DIR)/mod_pagespeed_ap24.so
	cp $(PAGESPEED_JS_MINIFY) $(STAGING_DIR)/pagespeed_js_minify

install_except_module : mod_pagespeed_file_root
	$(MODS_ENABLED_INSTALL_COMMANDS)
	cat $(CONF_SOURCES) > $(APACHE_CONF_DIR)/pagespeed.conf
	cp -f $(STAGING_DIR)/pagespeed_libraries.conf \
	      $(APACHE_CONF_D)/pagespeed_libraries.conf
	ln -Tsf $(STAGING_DIR)/mod_pagespeed_example \
	        $(APACHE_DOC_ROOT)/mod_pagespeed_example
	ln -Tsf $(STAGING_DIR)/mod_pagespeed_test \
	        $(APACHE_DOC_ROOT)/mod_pagespeed_test

# To install the mod_pagespeed configuration into the system, you must
# run this as root, or under sudo.
install : install_except_module
	cp $(STAGING_DIR)/mod_pagespeed.so $(APACHE_MODULES)
	cp $(STAGING_DIR)/mod_pagespeed_ap24.so $(APACHE_MODULES)
	cp $(STAGING_DIR)/pagespeed_js_minify $(BINDIR)

mod_pagespeed_file_root :
	mkdir -p $(MOD_PAGESPEED_CACHE)
	chown -R $(APACHE_USER) $(MOD_PAGESPEED_CACHE)

	mkdir -p $(MOD_PAGESPEED_CACHE)-alt
	chown -R $(APACHE_USER) $(MOD_PAGESPEED_CACHE)-alt

	mkdir -p $(MOD_PAGESPEED_LOG)
	chown -R $(APACHE_USER) $(MOD_PAGESPEED_LOG)

flush_disk_cache :
	rm -rf $(MOD_PAGESPEED_CACHE)
	$(MAKE) MOD_PAGESPEED_CACHE=$(MOD_PAGESPEED_CACHE) \
		MOD_PAGESPEED_LOG=$(MOD_PAGESPEED_LOG) \
		APACHE_USER=$(APACHE_USER) mod_pagespeed_file_root

# Starts Apache server
start : enable_ports_and_file_access
	sudo $(APACHE_START)
stop :
	sudo ./stop_apache.sh $(APACHE_CONTROL_PROGRAM) \
			      $(APACHE_PIDFILE) \
			      $(APACHE_PROGRAM) \
			      $(APACHE_STOP_COMMAND) \
			      $(APACHE_PORT)

# To run a complete iteration, stopping Apache, reconfiguring
# it, and and restarting it, you can run 'make restart [args...]
restart : stop enable_ports_and_file_access
	$(MAKE) staging
	sudo $(MAKE) install \
	    APACHE_DOC_ROOT=$(APACHE_DOC_ROOT) \
	    APACHE_ROOT=$(APACHE_ROOT) \
	    STAGING_DIR=$(STAGING_DIR) \
	    APACHE_CONF_FILE=$(APACHE_CONF_FILE) \
	    APACHE_MODULES=$(APACHE_MODULES) \
	    MODS_ENABLED_DIR=$(MODS_ENABLED_DIR) \
	    MODS_AVAILABLE_DIR=$(MODS_AVAILABLE_DIR) \
	    APACHE_USER=$(APACHE_USER) \
	    ENABLE_PROXY=$(ENABLE_PROXY)
	sudo $(APACHE_START)

# Tests that the installed mod_pagespeed server is working.
test :
	CACHE_FLUSH_TEST=on APACHE_DOC_ROOT=$(APACHE_DOC_ROOT) \
		../system_test.sh localhost

# Now hook in the full integration test suite. It needs to be run as root.
apache_install_conf :
	$(MAKE) staging_except_module $(OPTIONS)
	$(MAKE) install_except_module $(OPTIONS)

# Sets up a machine to run as a server for running mod_pagespeed tests.
# The tests include the fetching and proxying of a variety of content,
# including some of the /mod_pagespeed_example directory, plus some other
# content in /do_not_modify and /no_content.
setup_test_machine :
	APACHE_DOC_ROOT=$(APACHE_DOC_ROOT) \
	APACHE_CONF_FILE=$(APACHE_CONF_FILE) \
	MOD_PAGESPEED_ROOT=$(MOD_PAGESPEED_ROOT) \
	APACHE_CONF_D=$(APACHE_CONF_D) \
	./setup_test_machine.sh
	sudo $(APACHE_CONTROL_PROGRAM) restart

apache_debug_restart :
	$(APACHE_CONTROL_PROGRAM) restart

# The 'stop' command here is used for the 'install' target in order
# to ensure that apache is shut down when we are adding new debugging
# configuration and updating the binary.  The old binary might not
# be able to start up with new configuration-file commands.
apache_debug_install :
	$(APACHE_CONTROL_PROGRAM) stop

apache_debug_stop : stop

# Enables a few ports that are needed by system tests.  This is needed on
# CentOS only to work around barriers erected by SELinux.  See
# http://linux.die.net/man/8/semanage
# http://wiki.centos.org/HowTos/
# SELinux#head-ad837f60830442ae77a81aedd10c20305a811388
#
# The port-list below must be kept in sync with debug.conf.template.  1023 is
# used to test connection-refused handling via modpagespeed.com.  We don't
# actually create a VirtualHost on 1023.
enable_ports_and_file_access :
	if [ -x /usr/sbin/semanage ]; then \
	  set -x; \
	  for port in 1023 8081 8082 $(APACHE_SECONDARY_PORT) \
	      $(APACHE_TERTIARY_PORT) $(CONTROLLER_PORT); do \
	      sudo /usr/sbin/semanage port -a -t http_port_t -p tcp $$port || \
	      sudo /usr/sbin/semanage port -m -t http_port_t -p tcp $$port; \
	  done; \
	  for dir in $(MOD_PAGESPEED_CACHE) \
	             $(MOD_PAGESPEED_CACHE)-alt \
	             $(MOD_PAGESPEED_LOG) ; do \
	    sudo mkdir -p $$dir; \
	    sudo chown $(APACHE_USER) $$dir; \
	    sudo chcon -R --reference=$(APACHE_DOC_ROOT) $$dir || true; \
	  done; \
	  /usr/sbin/setsebool httpd_can_network_connect on; \
	fi

# Hooks for tests we can only run in development due to needing extensive
# configuration changes in Apache (and potentially different build flags).
# Stubbed out here.
apache_debug_leak_test :
apache_debug_proxy_test :
apache_debug_slurp_test :

APACHE_HTTPS_PORT=
APACHE_DEBUG_PAGESPEED_CONF=$(APACHE_CONF_DIR)/pagespeed.conf
INSTALL_DATA_DIR=.

include Makefile.tests
