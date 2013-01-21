#!/usr/bin/python
# Copyright 2013 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: oschaaf@gmail.com (Otto van der Schaaf)


# configuration generator for mod_pagespeed

# TODO(oschaaf): add license banner, author
# TODO(oschaaf): polish location matching
# TODO(oschaaf): change apache_literal to a literal in a condition block
# TODO(oschaaf): create a README
# TODO(oschaaf): list failing modes per server
# TODO(oschaaf): insert blurb with date/time, and flags in template header
# TODO(oschaaf): nginx + cgi
# TODO(oschaaf:  nginx + ssl
# TODO(oschaaf): wildcard locations?
# TODO(oschaaf): filesmatch stuff in pyconf
# TODO(oschaaf): check usage of proper formatting string everywhere
# TODO(oschaaf): fix long lines
# TODO(oschaaf): document

import sys
from genconf import execute_template

def exit_with_help_message():
    print "This script transforms .pyconf files into webserver configuration files"
    print "usage: ./genconf.py <output_format> <mode>"
    print "where output_format can be either 'apache' or 'nginx'"
    quit()

# conditions indicate which conditional configuration sections
# should be included
# formats:
#   #CONDITION
# or:
#   #ifdef CONDITION
#   ....
#   #endif
# the output mode ('nginx' or 'apache') is also set as a condition
# to be able to have output specific configuration sections
conditions = {}

# placeholders are inserted into the configuration during preprocessing
# format: @@PLACEHOLDER@@
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
