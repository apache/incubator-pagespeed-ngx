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
# In the case of the custom Apache build, you must also
# edit /usr/local/apache2/conf # to add "Include conf/extra/pagespeed.conf"
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
APACHE_MODULES = $(APACHE_ROOT)/modules

# The root directory Apache uses for serving files.
APACHE_DOC_ROOT = /var/www
# The domain Apache is serving from
#APACHE_DOMAIN = localhost:8080  # For test-server.
APACHE_DOMAIN = localhost
APACHE_HTTPS_DOMAIN = localhost

APACHE_CONTROL_PROGRAM = /etc/init.d/apache2
APACHE_START = $(APACHE_CONTROL_PROGRAM) start
APACHE_STOP = $(APACHE_CONTROL_PROGRAM) stop

# A temp directory to stage generated configuration files.  This must be
# writable by the user, and readable by root.
STAGING_DIR = /tmp/mod_pagespeed.install

# The mod_pagespeed module is specified relative to the install directory,
# which is src/install.
MOD_PAGESPEED_ROOT = ..
PAGESPEED_MODULE = $(MOD_PAGESPEED_ROOT)/out/Release/libmod_pagespeed.so

# On systems dervied from the NCSA configuration files by Rob McCool,
# you enable a module by writing its .conf file into
# $(APACHE_ROOT)/mods-enabled/pagespeed.conf, and a single Load command into
# $(APACHE_ROOT)/mods-enabled/pagespeed.conf.  So if that exists, then we'll
# try to automate that.
MODS_ENABLED_DIR = $(shell if [ -d $(APACHE_ROOT)/mods-enabled ]; then \
	echo $(APACHE_ROOT)/mods-enabled; fi)

# Determines should mod_pagespeed should write files
MOD_PAGESPEED_FILE_ROOT = /var/mod_pagespeed

# The username used to run apache.  This is needed to create the directory
# used to store mod_pagespeed files and cache data.
APACHE_USER = www-data

# Set this to 1 to enable mod_proxy and mod_rewrite
ENABLE_PROXY = 0

.PHONY: config_file echo_vars

echo_vars:
	@echo Run "restart" to add default instaweb config to apache
	@echo Or run "stop", "staging", "install", and "start".
	@echo These configuration variables can be reset on the make command line,
	@echo e.g. \"make config_file\"
	@echo ""
	@echo "   APACHE_CONF=$(APACHE_CONF)"
	@echo "   APACHE_MODULES=$(APACHE_MODULES)"
	@echo "   APACHE_ROOT=$(APACHE_ROOT)"
	@echo "   APACHE_START=$(APACHE_START)"
	@echo "   APACHE_STOP=$(APACHE_STOP)"
	@echo "   MOD_PAGESPEED_FILE_ROOT=$(MOD_PAGESPEED_FILE_ROOT)"
	@echo "   MODS_ENABLED_DIR=$(MODS_ENABLED_DIR)"
	@echo "   STAGING_DIR=$(STAGING_DIR)"
	@echo "   ENABLE_PROXY=${ENABLE_PROXY}"
	@echo "   SLURP_DIR=${SLURP_DIR}"


# In some Linux distributions, such as Ubuntu, there are two commands
# in the default root config file:
#    Include /etc/apache2/modes-enabled/*.load
#    Include /etc/apache2/modes-enabled/*.conf
# we need to write a one-line '.load' file and put that and our '.conf' file
# into .../mods-enabled.
#
# In other distributions, such as CentOS, there is an 'Include DIR/*.conf',
# but there is no implicit loading of modules, so we write our Load line
# directly into our config file

ifeq ($(MODS_ENABLED_DIR),)

# This is a CentOS-like installation, where there is no explicit .load
# file, and we instead pre-pend the LoadModule command to the .conf file.
APACHE_CONF = $(APACHE_ROOT)/conf.d
CONF_SOURCES = $(STAGING_DIR)/pagespeed.load $(STAGING_DIR)/pagespeed.conf

else

# This is an Ubuntu-like installation, where the .load files are placed
# separately into a mods-enabled directory, and the .conf file is loaded
# independently.
MODS_ENABLED_INSTALL_COMMANDS = \
	cp $(STAGING_DIR)/pagespeed.load $(MODS_ENABLED_DIR) ; \
	rm -f $(MODS_ENABLED_DIR)/headers.load ; \
	cd $(MODS_ENABLED_DIR) && ln -s ../mods-available/headers.load ; \
	rm -f $(MODS_ENABLED_DIR)/deflate.load ; \
	cd $(MODS_ENABLED_DIR) && ln -s ../mods-available/deflate.load

APACHE_CONF = $(MODS_ENABLED_DIR)
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

ifeq ($(STRESS_TEST),1)
  # remove prefix
  STRESS_TEST_SED_PATTERN=^\#STRESS
else
  # remove whole line
  STRESS_TEST_SED_PATTERN=^\#STRESS.*\n
endif

ifeq ($(REWRITE_TEST),1)
  # remove prefix
  REWRITE_TEST_SED_PATTERN=^\#REWRITE
else
  # remove whole line
  REWRITE_TEST_SED_PATTERN=^\#REWRITE.*\n
endif

ifeq ($(COVERAGE_TRACE_TEST),1)
  # remove coverage prefix
  COVERAGE_TEST_SED_PATTERN=^\#COVERAGE
else
  # remove coverage lines
  COVERAGE_TEST_SED_PATTERN=^\#COVERAGE.*\n
endif

ifeq ($(PROXY_TEST),1)
  # remove prefix
  PROXY_TEST_SED_PATTERN=^\#PROXY
else
  # remove whole line
  PROXY_TEST_SED_PATTERN=^\#PROXY.*\n
endif

ifeq ($(SLURP_TEST),1)
  # remove prefix
  SLURP_TEST_SED_PATTERN=^\#SLURP
else
  # remove whole line
  SLURP_TEST_SED_PATTERN=^\#SLURP.*\n
endif

ifeq ($(SPELING_TEST),1)
  # remove prefix
  SPELING_TEST_SED_PATTERN=^\#SPELING
else
  # remove whole line
  SPELING_TEST_SED_PATTERN=^\#SPELING.*\n
endif

ifeq ($(HTTPS_TEST),1)
  # remove prefix
  HTTPS_TEST_SED_PATTERN=^\#HTTPS
else
  # remove whole line
  HTTPS_TEST_SED_PATTERN=^\#HTTPS.*\n
endif

# Note that the quoted sed replacement for APACHE_SLURP_DIR_COMMAND is because
# that might have embedded spaces, and 'sed' is interpreted first by bash.

$(STAGING_DIR)/pagespeed.conf : common/pagespeed.conf.template debug.conf.template
	sed -e "s!@@APACHE_DOC_ROOT@@!$(APACHE_DOC_ROOT)!g" \
	    -e "s!@@APACHE_DOMAIN@@!$(APACHE_DOMAIN)!g" \
	    -e "s!@@APACHE_HTTPS_DOMAIN@@!$(APACHE_HTTPS_DOMAIN)!g" \
	    -e "s!@@APACHE_MODULES@@!$(APACHE_MODULES)!g" \
	    -e "s!@@MODPAGESPEED_CACHE_ROOT@@!$(MOD_PAGESPEED_FILE_ROOT)!g" \
	    -e "s@# ModPagespeedSlurpDirectory ...@$(APACHE_SLURP_DIR_COMMAND)@g" \
	    -e "s@# ModPagespeedSlurpReadOnly on@$(APACHE_SLURP_READ_ONLY_COMMAND)@g" \
	    -e "s@$(STRESS_TEST_SED_PATTERN)@@" \
	    -e "s@$(REWRITE_TEST_SED_PATTERN)@@" \
	    -e "s@$(COVERAGE_TEST_SED_PATTERN)@@" \
	    -e "s@$(PROXY_TEST_SED_PATTERN)@@" \
	    -e "s@$(SLURP_TEST_SED_PATTERN)@@" \
	    -e "s@$(SPELING_TEST_SED_PATTERN)@@" \
	    -e "s@$(HTTPS_TEST_SED_PATTERN)@@" \
		$^ > $@
	! grep '@@' $@  # Make sure we don't have any remaining @@variables@@

$(STAGING_DIR)/proxy.conf : proxy.conf.template
	sed -e s@APACHE_MODULES@$(APACHE_MODULES)@g \
		$< > $@

CONF_TEMPLATES = $(STAGING_DIR)/pagespeed.conf \
		 $(STAGING_DIR)/proxy.conf

setup_staging_dir :
	rm -rf $(STAGING_DIR)
	mkdir -p $(STAGING_DIR)

# Generate a configuration file and copy it to the staging area.
# Also copy the example tree, and the built Apache module
staging_except_module: setup_staging_dir $(CONF_TEMPLATES)
	echo "LoadModule pagespeed_module $(APACHE_MODULES)/mod_pagespeed.so" \
	 	> $(STAGING_DIR)/pagespeed.load
	echo "LoadModule deflate_module $(APACHE_MODULES)/mod_deflate.so" \
	 	>> $(STAGING_DIR)/pagespeed.load
	$(MODS_ENABLED_STAGING_COMMANDS)
	cp -rp mod_pagespeed_example mod_pagespeed_test $(STAGING_DIR)

staging: staging_except_module
	cp $(PAGESPEED_MODULE) $(STAGING_DIR)/mod_pagespeed.so

install_except_module : mod_pagespeed_file_root
	$(MODS_ENABLED_INSTALL_COMMANDS)
	cat $(CONF_SOURCES) > $(APACHE_CONF)/pagespeed.conf
	rm -rf $(APACHE_DOC_ROOT)/mod_pagespeed_example \
	       $(APACHE_DOC_ROOT)/mod_pagespeed_test
	cp -r $(STAGING_DIR)/mod_pagespeed_example \
	      $(STAGING_DIR)/mod_pagespeed_test $(APACHE_DOC_ROOT)
	chown -R $(APACHE_USER) $(APACHE_DOC_ROOT)/mod_pagespeed_example \
	 			$(APACHE_DOC_ROOT)/mod_pagespeed_test

# To install the mod_pagespeed configuration into the system, you must
# run this as root, or under sudo.
install : install_except_module
	cp $(STAGING_DIR)/mod_pagespeed.so $(APACHE_MODULES)

mod_pagespeed_file_root :
	mkdir -p $(MOD_PAGESPEED_FILE_ROOT)/cache
	mkdir -p $(MOD_PAGESPEED_FILE_ROOT)/files
	chown -R $(APACHE_USER) $(MOD_PAGESPEED_FILE_ROOT)

flush_disk_cache :
	rm -rf $(MOD_PAGESPEED_FILE_ROOT)
	$(MAKE) MOD_PAGESPEED_FILE_ROOT=$(MOD_PAGESPEED_FILE_ROOT) \
		APACHE_USER=$(APACHE_USER) mod_pagespeed_file_root

# Starts Apache server
start :
	sudo $(APACHE_START)
stop :
	sudo $(APACHE_STOP)

# To run a complete iteration, stopping Apache, reconfiguring
# it, and and restarting it, you can run 'make restart [args...]
restart : staging
	sudo $(APACHE_STOP)
	$(MAKE) staging
	sudo $(MAKE) install \
	    APACHE_DOC_ROOT=$(APACHE_DOC_ROOT) \
	    STAGING_DIR=$(STAGING_DIR) \
	    APACHE_CONF=$(APACHE_CONF) \
	    APACHE_MODULES=$(APACHE_MODULES) \
	    MODS_ENABLED_DIR=$(MODS_ENABLED_DIR) \
	    APACHE_USER=$(APACHE_USER) \
	    ENABLE_PROXY=$(ENABLE_PROXY)
	sudo $(APACHE_START)

# Tests that the installed mod_pagespeed server is working.
test :
	./system_test.sh localhost
	./apache_system_test.sh localhost

# Now hook in the full integration test suite. It needs to be run as root.
apache_install_conf:
	$(MAKE) staging_except_module $(OPT_REWRITE_TEST) $(OPT_PROXY_TEST) \
	                $(OPT_SLURP_TEST) $(OPT_SPELING_TEST) \
	                $(OPT_COVERAGE_TRACE_TEST) $(OPT_STRESS_TEST) \
	                $(OPT_HTTPS_TEST)
	$(MAKE) install_except_module $(OPT_REWRITE_TEST) $(OPT_PROXY_TEST) \
	                $(OPT_SLURP_TEST) $(OPT_SPELING_TEST) \
	                $(OPT_COVERAGE_TRACE_TEST) $(OPT_STRESS_TEST) \
	                $(OPT_HTTPS_TEST)

apache_debug_restart:
	$(APACHE_CONTROL_PROGRAM) restart

apache_debug_stop: stop

# Hooks for tests we can only run in development due to needing extensive
# configuration changes in Apache (and potentially different build flags).
# Stubbed out here.
apache_debug_leak_test:
apache_debug_proxy_test:
apache_debug_slurp_test:

APACHE_DEBUG_PORT=80
APACHE_HTTPS_PORT=
APACHE_CTRL_BIN=$(APACHE_CONTROL_PROGRAM)
APACHE_DEBUG_PAGESPEED_CONF=$(APACHE_CONF)/pagespeed.conf
PAGESPEED_ROOT=$(MOD_PAGESPEED_FILE_ROOT)
INSTALL_DATA_DIR=.

include tests.mk
