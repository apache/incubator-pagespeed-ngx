#!/usr/bin/env python

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
  "APACHE_DOC_ROOT" : "/drives/c/test/" 
}

output_format = "nginx"
mode = ""

if len(sys.argv) == 3:
    output_format = sys.argv[1]
    mode = sys.argv[2]

print("mode:" + mode);
print("format: " + output_format
)
if not (output_format == "apache" or output_format =="nginx" or output_format == "iis"):
    exit_with_help_message()

template = output_format + ".conf.template"
text = execute_template("pagespeed.pyconf", conditions, placeholders, template)

print text
