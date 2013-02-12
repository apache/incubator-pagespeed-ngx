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


# configuration generator for pagespeed

import datetime
import getpass
from genconf import execute_template
import sys
from util import Error

modes = [
    "ALL_DIRECTIVES",
    "COVERAGE",
    "FURIOUS_GA",
    "FURIOUS_NO_GA",
    "GZIP",
    "HTTPS",
    "MEMCACHE_COVERAGE",
    "MEMCACHED",
    "PER_VHOST_STATS",
    "PROXY",
    "REWRITE",
    "SHARED_MEM_LOCKS",
    "SLURP",
    "SPELING",
    "STATS_LOGGING",
    "STRESS"
    ]

def exit_with_help_message():
    print "This script transforms .pyconf files into webserver configuration files"
    print "usage: ./cli.py <input.pyconf> <output_format> <mode>"
    print "where output_format can be either 'apache' or 'nginx', and mode can be one of:"
    print "%s" % ", ".join(modes)
 
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
# format in templates: @@PLACEHOLDER@@
placeholders = {
    'APACHE_SECONDARY_PORT': 8083,
    'APACHE_DOMAIN': 'apache.domain',
    'APACHE_DOC_ROOT': '/home/oschaaf/code/google/mod_pagespeed/src/install',
    'MOD_PAGESPEED_CACHE': '/tmp/psol_cache',
    'APACHE_MODULES': '/usr/lib/apache2/modules',
    'NGX_CONF_DIRECTORY': '/usr/local/nginx/conf',
    'APACHE_SSL_CONF':'/etc/apache2/mods-available/ssl.conf',
    'APACHE_HTTPS_DOMAIN': 'localhost',
    'MOD_PAGESPEED_STATS_LOG': 'stats.log',
    'TMP_SLURP_DIR': '/tmp/instaweb/%s/slurp' % getpass.getuser()
    }

input_config_path = ""
output_format = ""
mode = ""

if len(sys.argv) == 4:
    input_config_path = sys.argv[1]
    output_format = sys.argv[2]
    mode = sys.argv[3]
else:
    exit_with_help_message()

conditions[mode] = True
conditions[output_format] = True
placeholders["__template_header"] = "generated at %s through \"%s\"" %\
    (datetime.datetime.now().strftime('%b-%d-%I%M%p-%G'), ' '.join(sys.argv))

# by convention, a file named <outputformat>.conf.template is
# expected to contain the translation script
template = output_format + '.conf.template'

text = execute_template(input_config_path, conditions,
                        placeholders, template)
print text

