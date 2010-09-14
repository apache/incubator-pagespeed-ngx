# This Makefile is used to help drive the installation of mod_instaweb into
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
# edit /usr/local/apache2/conf # to add "Include conf/extra/instaweb.conf"
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
# To install Instaweb properly, we need to know the locations of
# Apache configuration scripts and binaries.  These can are specified
# as Makefile variables which can be overridden on the command line.
# They have defaults, which will often need to be changed.


# The hostname used in URLs to find this Apache server.  This may be
# include a port, e.g.   www.yoursite.com or www.test.com:8080.  Specify
# this as HOSTNAME= when running Make.  This defaults to the result
# of running the Unix command 'hostname.  If you are going to enable
# domain sharding, then you must include a "%d" in the hostname so
# that we can construct sharded hostnames.
HOSTNAME = $(shell /bin/hostname)

# The location of the Apache root installation directory.  This helps form
# defaults for other variables, but each of those can be overridden.
APACHE_ROOT = /etc/httpd

# The installation directory for modules (mod*.so)
APACHE_MODULES = $(APACHE_ROOT)/modules

# The root directory Apache uses for serving files.
APACHE_DOC_ROOT = /var/www

APACHE_CONTROL_PROGRAM = /etc/init.d/apache2
APACHE_START = $(APACHE_CONTROL_PROGRAM) start
APACHE_STOP = $(APACHE_CONTROL_PROGRAM) stop

# A temp directory to stage generated configuration files.  This must be
# writable by the user, and readable by root.
STAGING_DIR = /tmp/instaweb.install

# The instaweb module is specified relative to the install directory,
# which is src/install.
INSTAWEB_ROOT = ..
INSTAWEB_MODULE = $(INSTAWEB_ROOT)/out/Release/libmod_instaweb.so

# On systems dervied from the NCSA configuration files by Rob McCool,
# you enable a module by writing its .conf file into 
# $(APACHE_ROOT)/mods-enabled/instaweb.conf, and a single Load command into
# $(APACHE_ROOT)/mods-enabled/instaweb.conf.  So if that exists, then we'll
# try to automate that.
MODS_ENABLED_DIR = $(shell if [ -d $(APACHE_ROOT)/mods-enabled ]; then \
	echo $(APACHE_ROOT)/mods-enabled; fi)

# Determines should Instaweb should write files
INSTAWEB_FILE_ROOT = /var/instaweb

# The username used to run apache.  This is needed to create the directory
# used to store instaweb files and cache data.
APACHE_USER = www-data

.PHONY: config_file echo_vars

echo_vars:
	@echo These configuration variables can be reset on the make command line,
	@echo e.g. \"make config_file HOSTNAME=www.yoursite.com\"
	@echo ""
	@echo "   HOSTNAME=$(HOSTNAME)"
	@echo "   APACHE_CONF=$(APACHE_CONF)"
	@echo "   APACHE_MODULES=$(APACHE_MODULES)"
	@echo "   APACHE_ROOT=$(APACHE_ROOT)"
	@echo "   APACHE_START=$(APACHE_START)"
	@echo "   APACHE_STOP=$(APACHE_STOP)"
	@echo "   INSTAWEB_FILE_ROOT=$(INSTAWEB_FILE_ROOT)"
	@echo "   MODS_ENABLED_DIR=$(MODS_ENABLED_DIR)"
	@echo "   STAGING_DIR=$(STAGING_DIR)"


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
CONF_SOURCES = $(STAGING_DIR)/instaweb.load $(STAGING_DIR)/instaweb.conf

else

# This is an Ubuntu-like installation, where the .load files are placed
# separately into a mods-enabled directory, and the .conf file is loaded
# independently.
MODS_ENABLED_INSTALL_COMMANDS = \
	cp $(STAGING_DIR)/instaweb.load $(MODS_ENABLED_DIR) ; \
	rm -f $(MODS_ENABLED_DIR)/headers.load ; \
	cd $(MODS_ENABLED_DIR) && ln -s ../mods-available/headers.load ; \
	rm -f $(MODS_ENABLED_DIR)/deflate.load ; \
	cd $(MODS_ENABLED_DIR) && ln -s ../mods-available/deflate.load

APACHE_CONF = $(MODS_ENABLED_DIR)
CONF_SOURCES = $(STAGING_DIR)/instaweb.conf

endif


# Generate a configuration file and copy it to the staging area.
# Also copy the example tree, and the built Apache module
staging:
	rm -rf $(STAGING_DIR)
	mkdir -p $(STAGING_DIR)
	echo "LoadModule instaweb_module $(APACHE_MODULES)/mod_instaweb.so" \
	 	> $(STAGING_DIR)/instaweb.load
	$(MODS_ENABLED_STAGING_COMMANDS)
	sed -e s@/HOSTNAME/@/$(HOSTNAME)/@g \
            -e s@APACHE_DOC_ROOT@$(APACHE_DOC_ROOT)@g \
	    -e s@INSTAWEB_FILE_ROOT@$(INSTAWEB_FILE_ROOT)@g \
		< instaweb.conf.template \
		> $(STAGING_DIR)/instaweb.conf
	cp -rp instaweb_example $(STAGING_DIR)
	cp $(INSTAWEB_MODULE) $(STAGING_DIR)/mod_instaweb.so

# To install the Instaweb configuration into the system, you must
# run this as root, or under sudo.
install : instaweb_file_root
	$(MODS_ENABLED_INSTALL_COMMANDS)
	cat $(CONF_SOURCES) > $(APACHE_CONF)/instaweb.conf
	rm -rf $(APACHE_DOC_ROOT)/instaweb_example
	cp -r $(STAGING_DIR)/instaweb_example $(APACHE_DOC_ROOT)
	chown -R $(APACHE_USER) $(APACHE_DOC_ROOT)/instaweb_example
	cp $(STAGING_DIR)/mod_instaweb.so $(APACHE_MODULES)

instaweb_file_root :
	mkdir -p $(INSTAWEB_FILE_ROOT)/cache
	mkdir -p $(INSTAWEB_FILE_ROOT)/files
	chown -R $(APACHE_USER) $(INSTAWEB_FILE_ROOT)

flush_disk_cache :
	rm -rf $(INSTAWEB_FILE_ROOT)
	$(MAKE) INSTAWEB_FILE_ROOT=$(INSTAWEB_FILE_ROOT) \
		APACHE_USER=$(APACHE_USER) instaweb_file_root

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
	    APACHE_USER=$(APACHE_USER)
	sudo $(APACHE_START)

# Tests that the installed Instaweb server is working.
test : verify_instaweb_header verify_css_files_combined

SAMPLE_URL = http://$(HOSTNAME)/instaweb_example/combine_mixed.html

# Verify that HTML files are served with the expected Instaweb header.
verify_instaweb_header :
	wget -O - --save-headers $(SAMPLE_URL) 2>&1 | grep x-instaweb
	@echo Instaweb is running in Apache and returning the expected header

TMP_DIR = /tmp

# Verify that Instaweb successfully combines four CSS files into one.
verify_css_files_combined :
	wget -O - $(SAMPLE_URL) > /dev/null 2>&1
	wget -O - $(SAMPLE_URL) 2>&1 | grep text/css | wc -l > $(TMP_DIR)/css_count.txt
	echo 1 > $(TMP_DIR)/css_count_expected.txt
	diff $(TMP_DIR)/css_count.txt $(TMP_DIR)/css_count_expected.txt
	rm -f $(TMP_DIR)/css_count.txt $(TMP_DIR)/css_count_expected.txt
	@echo Instaweb successfully combined 4 CSS files into 1
