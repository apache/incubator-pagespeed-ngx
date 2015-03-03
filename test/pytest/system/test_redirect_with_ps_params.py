import pytest

import config
import test_helpers as helpers

# Test that redirecting to the same domain retains MPS query parameters.
# The test domain is configured for collapse_whitepsace,add_instrumentation
# so if the QPs are retained we should get the former but not the latter.
proxy = config.SECONDARY_SERVER

@pytest.mark.skipif("not proxy")
def test_redirecting_to_the_same_domain_retains_pagespeed_query_parameters():
    url  = "http://redirect.example.com/mod_pagespeed_test/forbidden.html"
    opts =  "?PageSpeedFilters=-add_instrumentation"
    # First, fetch with add_instrumentation enabled (default) to ensure it is on
    result = helpers.fetch(url, proxy=proxy)
    assert result.resp.status != 301 and result.resp.status != 302
    assert not result.resp.getheader("location")
    assert result.body.count("pagespeed.addInstrumentationInit")
    assert not result.body.count("  ")

    # Then, fetch with add_instrumentation disabled and the URL not redirected.
    result = helpers.fetch("%s%s" % (url, opts), proxy=proxy)
    assert result.resp.status != 301 and result.resp.status != 302
    assert not result.resp.getheader("location")
    assert not result.body.count("pagespeed.addInstrumentationInit")
    assert not result.body.count("  ")

    # Finally, fetch with add_instrumentation disabled and the URL redirected.
    url  = ("http://redirect.example.com/redirect/mod_pagespeed_test/"
        "forbidden.html%s" % opts)
    result = helpers.fetch("%s%s" % (url, opts), proxy=proxy)
    assert result.resp.status == 301 or result.resp.status == 302
    assert result.resp.getheader("location")
    assert result.resp.getheader("location").count("-add_instrumentati")
    assert not result.body.count("pagespeed.addInstrumentationInit")
    assert not result.body.count("  ")

