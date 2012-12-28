#!/usr/bin/env python

import sys
from genconf import execute_template

conditions = {
  #"ALL_DIRECTIVES":1,
  #"MEMCACHED":1,
  #"PER_VHOST_STATS":1,
  #"STATS_LOGGING":1,
}

placeholders = { 
  "APACHE_DOC_ROOT" : "/drives/c/test/" 
}

mode = ""

if len(sys.argv) == 2:
    mode = sys.argv[1]

if not (mode == "apache" or mode =="nginx" or mode == "iis"):
    print("usage: ./genconf.py <mode>")
    print("where mode can be either 'apache' or 'nginx'")
    quit()

template = mode + ".conf.template"
text = execute_template("pagespeed.pyconf", conditions, placeholders, template)

print text
