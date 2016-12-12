#!/usr/bin/python
#
# Copyright 2010 Google Inc.
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
"""Fetches a set of URLs via a proxy, keeping statistics.

This script attempts to fetch all URLs in the list given on
the command-line via a specified proxy. It differs from the
widely available tools in that:
- The proxy connection is kept-alive to try to maximize throughput.
- Statuses and completion times for each URL are output to stdout to
  help analyze the results.

With the --js option the output is a JavaScript object literal with fields named
for URLs with http:// replaced with whatever is passed as test_cat,
followed by a dash.

"""

__author__ = "morlovich@google.com (Maksim Orlovich)"

import getopt
import httplib
import socket
import sys
import time
import urlparse


def OpenProxy(config):
  if config.ssl_mode:
    new_proxy = httplib.HTTPSConnection(config.proxy_host, config.proxy_port)
  else:
    new_proxy = httplib.HTTPConnection(config.proxy_host, config.proxy_port)
  new_proxy.connect()
  return new_proxy


def ReopenProxy(config, old_proxy):
  old_proxy.close()
  return OpenProxy(config)


def TestName(config, test_url):
  return test_url.replace("http://", config.test_cat + "-")


def FormatResult(config, time_str, status, test_url):
  if config.js_mode:
    return '"%s": %s,' % (TestName(config, test_url), time_str)
  else:
    return "%s %s %s" % (time_str, status, test_url)


class Configuration(object):
  """packages up execution settings."""

  def __init__(self):
    """Initializes settings from command-line arguments."""
    try:
      opts, _ = getopt.getopt(sys.argv[1:], "",
                              ["ssl", "js=", "proxy_host=", "proxy_port=",
                               "urls_file=", "user_agent="])
    except getopt.GetoptError as err:
      print str(err)
      print ("Usage: devel/fetch_all.py [--ssl] [--js test_cat] "
             "[--proxy_host host] [--proxy_port port] [--urls_file file] "
             "[--user_agent user_agent]")
      sys.exit(2)

    self.ssl_mode = False
    self.js_mode = False
    self.has_user_agent = False

    for name, value in opts:
      if name == "--ssl":
        self.ssl_mode = True
      elif name == "--js":
        self.js_mode = True
        self.test_cat = value
      elif name == "--proxy_host":
        self.proxy_host = value
      elif name == "--proxy_port":
        self.proxy_port = int(value)
      elif name == "--urls_file":
        self.urls_file = value
      elif name == "--user_agent":
        self.has_user_agent = True
        self.user_agent = value


def main():
  conf = Configuration()

  # Open a persistent connection to the proxy
  proxy = OpenProxy(conf)

  if conf.js_mode:
    print "{"

  f = open(conf.urls_file, "rt")
  for url in f:
    try:
      # Fetch url
      status = 301
      followed = 0
      while followed < 5:
        url = url.strip()
        if conf.ssl_mode:
          url = url.replace("http://", "https://", 1)
        start = time.time()

        headers = {"Accept-Encoding": "gzip"}
        if conf.has_user_agent:
          headers["User-Agent"] = conf.user_agent
          if "Chrome/" in conf.user_agent:
            headers["Accept"] = "image/webp"
        proxy.request("GET", url, None, headers)

        response = proxy.getresponse()
        response.read()
        stop = time.time()
        status = response.status

        # Honor server's close request
        connect_ctl = response.getheader("connection", default="")
        if connect_ctl.lower().find("close") != -1:
          proxy = ReopenProxy(conf, proxy)

        # Report.
        print FormatResult(conf, str((stop - start)*1000),
                           str(status), url)

        # Handle redirections
        if 301 <= status <= 303 or status == 307:
          url = urlparse.urljoin(url,
                                 response.getheader("Location", default=""))
          followed += 1
        else:
          break
    except httplib.BadStatusLine:
      print FormatResult(conf, "0", "BadStatusLine", url)
      proxy = ReopenProxy(conf, proxy)
    except httplib.IncompleteRead:
      print FormatResult(conf, "0", "IncompleteRead", url)
      proxy = ReopenProxy(conf, proxy)
    except socket.error:
      print FormatResult(conf, "0", "SocketError", url)
      proxy = ReopenProxy(conf, proxy)

  if conf.js_mode:
    print "}"

  f.close()

if __name__ == "__main__":
  main()
