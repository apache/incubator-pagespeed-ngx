import config
import test_helpers as helpers



def test_extend_cache_with_no_cache_js_origin():
    # Test that we can rewrite resources that are served with
    # Cache-Control: no-cache with on-the-fly filters.  Tests that the
    # no-cache header is preserved.
    url = "%s/no_cache/hello.js.pagespeed.ce.0.js" % config.REWRITTEN_TEST_ROOT

    resp, body = helpers.fetch(url)
    assert body.count('Hello')
    assert resp.getheader("cache-control").count("no-cache")


def test_can_rewrite_cache_control_no_cache_resources_non_on_the_fly_filters():
    # Test that we can rewrite Cache-Control: no-cache resources with
    # non-on-the-fly filters.
    url = "%s/no_cache/hello.js.pagespeed.jm.0.js" % config.REWRITTEN_TEST_ROOT

    resp, body = helpers.fetch(url)
    assert body.count('Hello')
    assert resp.getheader("cache-control").count("no-cache")
