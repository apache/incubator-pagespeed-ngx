#!/usr/bin/env python
# configuration generator for mod_pagespeed


import sys
from genconf import execute_template

def exit_with_help_message():
    print("usage: ./genconf.py <output_format> <mode>")
    print("where output_format can be either 'apache' or 'nginx' or 'iis'")
    print("and mode can be one of @@TODO")
    quit()


conditions = {
  #"ALL_DIRECTIVES":1,
  #"MEMCACHED":1,
  #"PER_VHOST_STATS":1,
  #"STATS_LOGGING":1,
}

placeholders = { 
    "APACHE_SECONDARY_PORT":8083,
    "APACHE_DOMAIN": "apache.domain",
    "APACHE_DOC_ROOT": "/var/www",
    "MOD_PAGESPEED_CACHE":"/tmp/psol_cache",
}

output_format = ""
mode = ""

if len(sys.argv) == 3:
    output_format = sys.argv[1]
    mode = sys.argv[2]


#print("mode:" + mode);
#print("format: " + output_format)
conditions[mode]=1
if not (output_format == "apache" or output_format =="nginx" or output_format == "iis" or output_format == "apache2"
        or output_format == "nginx2"):
    exit_with_help_message()

template = output_format + ".conf.template"
#text = execute_template("pagespeed.pyconf", conditions, placeholders, template)
text = execute_template("debug.conf.template", conditions, placeholders, template)

print text
