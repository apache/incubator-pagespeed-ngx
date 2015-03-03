import re

import config
import test_helpers as helpers

# TODO(oschaaf): finish this list.
filters = [
    ("add_instrumentation","ai"),
    ("canonicalize_javascript_libraries","ij"),
    ("collapse_whitespace","cw"),
    ("combine_css","cc"),
    ("combine_heads","ch"),
]

def inspect_via_debug_filter(url, filter_code, headers = None):
    _resp, body = helpers.fetch(url, headers = headers)

    # Debug filter enabled
    assert body.count("#NumFlushes") == 1

    # normalize line endings
    body = body.replace("\r","")

    # debug filter emits the enabled filters
    pattern = r'Filters:\n(.*?)\n\n'
    match = re.search(pattern, body, re.DOTALL)

    # Validate that the debug filter states the specified filter was enabled.
    assert match
    print match.group(1)
    expect = "%s\t" % filter_code
    assert match.group(1).count(expect) == 1


def inspect_via_rewriting(url, filter_name, headers = None):
    # Now compare the passthrough output to the output with the filter enabled
    # As all examples are supposed to be doing something, there should be a
    # difference.
    url_passthrough = ("%s/%s.html?PageSpeedFilters=" %
        (config.EXAMPLE_ROOT, filter_name))
    _passthrough_resp, passthrough_body = helpers.fetch(url_passthrough)

    # No debug filter enabled
    assert passthrough_body.count("#NumFlushes") == 0

    # Next, we'll do a blocking fetch with the specified filter enabled.
    # As all examples are supposed to do domething, the output should be
    # different.
    if headers == None:
      headers = {}
    headers["X-PSA-Blocking-Rewrite"] = "psatest"

    _resp, body = helpers.fetch(url, headers = headers)
    # No debug filter enabled
    assert body.count("#NumFlushes") == 0

    assert body != passthrough_body


def test_pagespeed_query_input():
    for filter_name, filter_code in filters:
        url = ("%s/%s.html?PageSpeedFilters=debug,%s"
            % (config.EXAMPLE_ROOT, filter_name, filter_name))
        inspect_via_debug_filter(url, filter_code)

        url = ("%s/%s.html?PageSpeedFilters=%s"
            % (config.EXAMPLE_ROOT, filter_name, filter_name))
        inspect_via_rewriting(url, filter_name)

def test_modpagespeed_query_input():
    for filter_name, filter_code in filters:
        url = ("%s/%s.html?ModPagespeedFilters=debug,%s"
             % (config.EXAMPLE_ROOT, filter_name, filter_name))
        inspect_via_debug_filter(url, filter_code)

        url = ("%s/%s.html?ModPagespeedFilters=%s"
             % (config.EXAMPLE_ROOT, filter_name, filter_name))
        inspect_via_rewriting(url, filter_name)

def test_pagespeed_request_header_input():
    for filter_name, filter_code in filters:
        url = "%s/%s.html" % (config.EXAMPLE_ROOT, filter_name)

        headers = {"PageSpeedFilters" : "debug,%s" % filter_name}
        inspect_via_debug_filter(url, filter_code, headers)

        headers = {"PageSpeedFilters" : "%s" % filter_name}
        inspect_via_rewriting(url, filter_name, headers)

def test_modpagespeed_request_header_input():
    for filter_name, filter_code in filters:
        url = "%s/%s.html" % (config.EXAMPLE_ROOT, filter_name)

        headers = {"ModPagespeedFilters" : "debug,%s" % filter_name}
        inspect_via_debug_filter(url, filter_code, headers)

        headers = {"ModPagespeedFilters" : "%s" % filter_name}
        inspect_via_rewriting(url, filter_name, headers)



