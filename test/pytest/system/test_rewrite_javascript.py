import re
import urllib3

import config
import test_helpers as helpers


def test_rewrite_javascript_minifies_javascript_and_saves_bytes():
    url = ("%s/rewrite_javascript.html?PageSpeedFilters=rewrite_javascript" %
        config.EXAMPLE_ROOT)
    pattern = r'src=.*rewrite_javascript\.js\.pagespeed\.jm\.'
    # External scripts rewritten.
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.patternCountEquals, pattern, 2)
    assert success, result.body
    body = result.body
    results = re.findall(r'".*.pagespeed.jm[^"]*', body)
    results = [x[1:] for x in results]
    # If PreserveUrlRelativity is on, we need to find the relative URL and
    # absolutify it ourselves.
    results = [helpers.absolutify_url(url, u) for u in results]


    for result in results:
        print result
        js_resp, js_body = helpers.fetch(result)
        assert js_body.count("removed") == 0  # No comments should remain.
        # Rewritten JS is cache-extended.
        assert js_resp.getheader("cache-control") == "max-age=31536000"
        assert js_resp.getheader("expires")

    assert len(body) < 1560             # Net savings
    assert body.count("preserved") > 0  # Preserves certain comments.


def test_rewrite_javascript_external():
    url = ("%s/rewrite_javascript.html?PageSpeedFilters="
        "rewrite_javascript_external" % config.EXAMPLE_ROOT)
    pattern = r'src=.*rewrite_javascript\.js\.pagespeed\.jm\.'

    result, success = helpers.FetchUntil(url).waitFor(
        helpers.patternCountEquals, pattern, 2)
    assert success, result.body
    assert result.body.count("// This comment will be removed") > 0


def test_rewrite_javascript_inline():
    url = ("%s/rewrite_javascript.html?PageSpeedFilters="
        "rewrite_javascript_inline" % config.EXAMPLE_ROOT)
    # We test with blocking rewrites here because we are trying to prove
    # we will never rewrite the external JS, which is impractical to do
    # with FetchUntil.
    _resp, body = helpers.fetch(url,
        {"X-PSA-Blocking-Rewrite": "psatest"})
    # Checks that the inline JS block was minified.
    assert body.count("// This comment will be removed") == 0
    assert body.count('id="int1">var state=0;document.write') > 0
    # Checks that the external JS links were left alone.
    assert body.count('src="rewrite_javascript.js') == 2
    pattern = r'src=.*rewrite_javascript\.js\.pagespeed\.jm\.'
    assert len(re.findall(pattern, body)) == 0


# Error path for fetch of outlined resources that are not in cache leaked
# at one point of development.
def test_regression_test_for_RewriteDriver_leak():
    url = "%s/_.pagespeed.jo.3tPymVdi9b.js" % config.TEST_ROOT
    result = helpers.fetch(url, allow_error_responses = True)
    assert result.resp.status == 404

# Combination rewrite in which the same URL occurred twice used to
# lead to a large delay due to overly late lock release.
def test_regression_test_with_same_filtered_input_twice_in_combination():
    url = ("%s/_,Mco.0.css+_,Mco.0.css.pagespeed.cc.0.css?PageSpeedFilters="
        "combine_css,outline_css" % config.TEST_ROOT)
    try:
        result = helpers.fetch(url, timeout=urllib3.Timeout(read=3),
            allow_error_responses = True)
        assert result.resp.status == 404
    except urllib3.exceptions.ReadTimeoutError as ex:
        assert 0, ex
