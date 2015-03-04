import re

import config
import test_helpers as helpers

proxy = config.SECONDARY_SERVER
# TODO(oschaaf): _ in host? nginx will respond with status 400 if that exists
# in the request line, we must specify this as a host header to pass.
host = "proxy_cache.example.com"

headers = {"Accept-Encoding" : "gzip", "Host" : host}

# TODO(oschaaf): skip if native fetcher in use(?)
def test_downstream_cache():
    cachable_html_loc = ("/mod_pagespeed_test/cachable_rewritten_html/"
                        "downstream_caching.html")
    tmp_log_line = ("%s GET /purge/mod_pagespeed_test/"
                "cachable_rewritten_html/downstream_caching.html.*(200)" % host)

    helpers.assert_stat_equals("downstream_cache_purge_attempts", 0)

    # The 1st request results in a cache miss, non-rewritten response
    # produced by pagespeed code and a subsequent purge request.
    #start_test Check for case where rewritten cache should get purged.
    result = helpers.fetch(cachable_html_loc, headers = headers, proxy = proxy)
    # We shouldn't have been rewriting
    assert result.body.count("pagespeed.ic") == 0
    assert result.resp.getheader("x-cache") == "MISS"
    helpers.assert_wait_for_stat_to_equal("downstream_cache_purge_attempts", 1)
    with open(config.ACCESS_LOG) as f:
        assert len(re.findall(tmp_log_line, f.read(), re.MULTILINE)) == 1



    # The 2nd request results in a cache miss (because of the previous purge),
    # rewritten response produced by pagespeed code and no new purge requests.
    headers["X-PSA-Blocking-Rewrite"] = "psatest"
    result = helpers.fetch(cachable_html_loc, headers = headers, proxy = proxy)

    # We should have been rewriting
    assert result.body.count("pagespeed.ic")
    assert result.resp.getheader("x-cache") == "MISS"

    helpers.assert_stat_equals("downstream_cache_purge_attempts", 1)
    with open(config.ACCESS_LOG) as f:
        assert len(re.findall(tmp_log_line, f.read(), re.MULTILINE)) == 1

    del headers["X-PSA-Blocking-Rewrite"]

    # The 3rd request results in a cache hit (because the previous response is
    # now present in cache), rewritten response served out from cache and not
    # by pagespeed code and no new purge requests.
    result = helpers.fetch(cachable_html_loc, headers = headers, proxy = proxy)
    # We should have been rewriting
    assert result.body.count("pagespeed.ic")
    assert result.resp.getheader("x-cache") == "HIT"

    # TODO(oschaaf): why does this need a fetch_until?
    helpers.assert_wait_for_stat_to_equal("downstream_cache_purge_attempts", 1)
    with open(config.ACCESS_LOG) as f:
        assert len(re.findall(tmp_log_line, f.read(), re.MULTILINE)) == 1


    # Enable one of the beaconing dependent filters and verify interaction
    # between beaconing and downstream caching logic, by verifying that
    # whenever beaconing code is present in the rewritten page, the
    # output is also marked as a cache-miss, indicating that the instrumentation
    # was done by the backend.
    headers["X-Allow-Beacon"] = "yes"
    cachable_html_loc = ("%s?PageSpeedFilters=lazyload_images"
        % cachable_html_loc)

    result, success = helpers.FetchUntil(
        cachable_html_loc, headers = headers, proxy = proxy).waitFor(
        helpers.stringCountEquals, "pagespeed.CriticalImages.Run", 2)
    assert success, result.body
    assert result.resp.getheader("x-cache") == "BYPASS"
    assert result.resp.getheader("cache-control") == "no-cache, max-age=0"

