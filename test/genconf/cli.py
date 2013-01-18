#!/usr/bin/python
# -*- coding: utf-8 -*-

# configuration generator for mod_pagespeed

import sys
from genconf import execute_template


def exit_with_help_message():
    print 'usage: ./genconf.py <output_format> <mode>'
    print "where output_format can be either 'apache' or 'nginx'"
    print 'and mode can be one of '
    quit()


conditions = {}
placeholders = {
    'APACHE_SECONDARY_PORT': 8083,
    'APACHE_DOMAIN': 'apache.domain',
    'APACHE_DOC_ROOT': '/home/oschaaf/code/google/mod_pagespeed/src/install',
    'MOD_PAGESPEED_CACHE': '/tmp/psol_cache',
    'APACHE_MODULES': '/usr/lib/apache2/modules',
    'NGX_CONF_DIRECTORY': '/usr/local/nginx/conf',
    'APACHE_SSL_CONF':'/etc/apache2/mods-available/ssl.conf',
    'APACHE_HTTPS_DOMAIN': 'localhost',
    'MOD_PAGESPEED_STATS_LOG': 'stats.log'
    }

output_format = ''
mode = ''

if len(sys.argv) == 3:
    output_format = sys.argv[1]
    mode = sys.argv[2]
else:
    exit_with_help_message()

conditions[mode] = True
conditions[output_format] = True

template = output_format + '.conf.template'
text = execute_template('pagespeed.debug.pyconf', conditions,
                        placeholders, template)

print text
