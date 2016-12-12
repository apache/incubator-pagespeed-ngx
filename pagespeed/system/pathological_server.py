#!/usr/bin/python
#
# Copyright 2016 Google Inc.
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
"""Simple python server for testing remote_config in tricky situations.

Usage:
  1) Start pathological_server.py <port> in the background
  2) Send queries to localhost:port/filename
     * filename will be looked up in responses below to get the main
       response body.
     * if there's a hook, that's run right before sending the response out.
       Hooks can do things like delay the response or change the values returned
       for future requests.

Author: Jeff Kaufman (jefftk@google.com)
"""

import select
import socket
import sys
import time


def _fail_future_requests(filename):
  """Make future requests for this file 410."""
  def _helper(ps, _):
    ps.set_response(
        filename,
        "HTTP/1.1 410 Gone\r\n"
        "\r\n"
        "This webserver has been instructed to fail further requests for this "
        "resource\n",
        _nohook)
  return _helper


def _return_on_future_requests(filename, response):
  """Set the response for future requests for this file."""
  def _helper(ps, _):
    ps.set_response(filename, response, _nohook)
  return _helper


def _wait_before_serving(seconds):
  """Tell the server not to write to this socket for the specified time."""
  def _helper(ps, soc):
    ps.delay_writing_for(seconds * 1000, soc)
  return _helper


def _nowms():
  """Current time in milliseconds."""
  return int(time.time() * 1000)


_RECV_CHUNK_SIZE = 2048
_STANDARD_CONFIG = (
    "HTTP/1.1 200 OK\r\n"
    "Cache-Control: max-age=5\r\n"
    "\r\n"
    "EnableFilters remove_comments,collapse_whitespace\n"
    "EndRemoteConfig\n")

# filename -> (response_to_send, hook)
_nohook = lambda *_: None
_responses = {
    "/standard": (_STANDARD_CONFIG, _nohook),

    "/partly-invalid": (
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=5\r\n"
        "\r\n"
        "ThisIsntValidPageSpeedConf\n"
        "EnableFilters remove_comments,collapse_whitespace\n"
        "ThisIsntValidPageSpeedConfEither\n"
        "EndRemoteConfig\n",
        _nohook),

    "/invalid": (
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=5\r\n"
        "\r\n"
        "EnableFilters remove_comments,collapse_whitespace\n",
        _nohook),

    "/out-of-scope": (
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=5\r\n"
        "\r\n"
        "UrlSigningKey secretkey\n"
        "RequestOptionOverride secretkey\n"
        "EndRemoteConfig\n",
        _nohook),

    "/fail-future": (_STANDARD_CONFIG, _fail_future_requests("/fail-future")),

    "/timeout": (
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=2\r\n"
        "\r\n"
        "EnableFilters remove_comments,collapse_whitespace\n"
        "EndRemoteConfig\n",
        # Wait impossibly long to reply.  Any config depending on this url won't
        # load.
        _wait_before_serving(10000)),

    "/experiment": (
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=5\r\n"
        "\r\n"
        "RunExperiment on\n"
        "AnalyticsID UA-MyExperimentID-1\n"
        "UseAnalyticsJs false\n"
        "EndRemoteConfig\n",
        _nohook),

    "/slightly-slow": (
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=20\r\n"
        "\r\n"
        "EnableFilters remove_comments,collapse_whitespace\n"
        "EndRemoteConfig\n",
        # Wait a short while.  A config depending on this will load successfully
        # if we're handling background refreshes but not otherwise.
        _wait_before_serving(2)),

    "/slow-expired": (
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=0\r\n"
        "\r\n"
        "EnableFilters remove_comments,collapse_whitespace\n"
        "EndRemoteConfig\n",
        # ditto
        _wait_before_serving(2)),

    "/forbidden": (
        "HTTP/1.1 403 Forbidden\r\n"
        "Cache-Control: max-age=5\r\n"
        "\r\n"
        "EnableFilters remove_comments,collapse_whitespace\n"
        "EndRemoteConfig\n",
        _nohook),

    "/forbidden-once": (
        "HTTP/1.1 403 Forbidden\r\n"
        "Cache-Control: max-age=5\r\n"
        "\r\n",
        _return_on_future_requests("/forbidden-once", _STANDARD_CONFIG)),
}


class PathologicalServer(object):
  """Simple test webserver, serving the specified responses.

  Implements just enough of HTTP to handle what we need:
  * Only handles GET requests.
  * Doesn't bother to set a content-length, and just closes the connection
    instead.
  * Doesn't validate that it's being sent reasonable headers.
  """

  def __init__(self, host, port, responses):
    self._host = host
    self._port = int(port)
    self._responses = responses

    self._read_list = []
    self._write_list = []
    self._timer_list = []  # (time-to-resume_ms, function-to-run)

    self._reading = {}  # socket -> data read
    self._writing = {}  # socket -> data to write

  def start(self):
    """Initialize, listen, and begin an event loop.  Doesn't return."""
    self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    self._server_socket.setblocking(0)  # make it non-blocking
    self._server_socket.bind((self._host, self._port))
    # Allow queuing 5 requests, not that it matters.
    self._server_socket.listen(5)
    self._read_list.append(self._server_socket)

    while True:
      readable, writable, errored = select.select(
          self._read_list, self._write_list, [], 0.1)

      timer_entries_to_remove = []
      for entry in self._timer_list:
        time_to_resume_ms, function_to_run = entry
        if _nowms() > time_to_resume_ms:
          timer_entries_to_remove.append(entry)
          function_to_run()
      for entry in timer_entries_to_remove:
        self._timer_list.remove(entry)

      for soc in readable:
        if soc is self._server_socket:
          client_socket, _ = soc.accept()
          self._read_list.append(client_socket)
        else:
          self._handle_reading(soc)

      for soc in writable:
        self._handle_writing(soc)

      for soc in errored:
        self._handle_error(soc)

  def delay_writing_for(self, ms, soc):
    """Delay any response on this socket for the specified time.

    This is handled by moving the socket temporarily from the list of sockets
    that need writing to the list of ones that need waiting.

    Args:
      ms: duration in milliseconds
      soc: which socket to delay
    """
    self._log("waiting %sms before responding..." % ms)

    def resume_writing():
      self._write_list.append(soc)

    self._write_list.remove(soc)
    self._timer_list.append((_nowms() + ms, resume_writing))

  def set_response(self, filename, response_text, response_hook):
    """Choose how we'll respond when we get future requests for this file."""
    self._responses[filename] = response_text, response_hook

  def _handle_error(self, soc):
    """Log an error with a socket, and then clean it up."""
    err_string = "socket error"
    if soc in self._reading:
      err_string += (" with '%s' read" % self._reading[soc])
    if soc in self._writing:
      err_string += (" with '%s' still to write" % self._writing[soc])
    self._log_error(err_string)
    self._cleanup(soc)

  def _cleanup(self, soc):
    """Close the socket and stop tracking it."""
    soc.close()
    for l in [self._read_list, self._write_list]:
      if soc in l:
        l.remove(soc)
    for d in [self._reading, self._writing]:
      if soc in d:
        del d[soc]
    for time_ms, timer_soc in self._timer_list:
      if soc is timer_soc:
        self._log_error("cleaning up socket with %sms remaining" % (time_ms))

  def _handle_reading(self, soc):
    """Given a socket with something to read, read what's available."""
    chunk = soc.recv(_RECV_CHUNK_SIZE)
    if not chunk:
      self._handle_error(soc)  # unexpected EOF
      return
    if soc not in self._reading:
      self._reading[soc] = ""
    self._reading[soc] += chunk.decode("utf-8")

    if self._reading[soc].endswith("\r\n\r\n"):
      # Finished reading request headers, don't expect request body.
      self._log("read %r" % self._reading[soc])
      headers = self._reading[soc]
      self._reading[soc] = ""

      if not headers.startswith("GET "):
        raise Exception("Only GET requests are supported.")
      self._writing[soc], hook = self._responses[headers.split()[1]]

      # Move the socket to list of things waiting to write.
      self._read_list.remove(soc)
      self._write_list.append(soc)

      hook(self, soc)

  def _handle_writing(self, soc):
    """Write as much as the socket will let us."""
    self._log("writing %r" % self._writing[soc])
    sent = soc.send(self._writing[soc])
    if not sent:
      self._handle_error(soc)
    # Offsets would be more efficient, but this is python so it's not worth it.
    self._writing[soc] = self._writing[soc][sent:]
    if not self._writing[soc]:
      # Finished writing the whole thing.
      self._cleanup(soc)

  def _log(self, s):
    sys.stdout.write("[server %s] %s\n" % (_nowms(), s))
    sys.stdout.flush()

  def _log_error(self, s):
    sys.stderr.write("[server error %s] %s" % (_nowms(), s))


def main(port):
  """Start a PathologicalServer on the specified port, bound to localhost."""
  ps = PathologicalServer("localhost", port, _responses)
  ps.start()


if __name__ == "__main__":
  main(*sys.argv[1:])
