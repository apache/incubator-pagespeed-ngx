import pytest

import config
import test_helpers as helpers




def test_inline_css_converts_3_out_of_5_link_tags_to_style_tags():
    url = ("%s/inline_css.html?PageSpeedFilters=inline_css" %
        config.EXAMPLE_ROOT)
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "<style", 3)
    assert success, result.body

# In some test environments these tests can't be run because of
# restrictions on external connections
@pytest.mark.skipif("config.DISABLE_FONT_API_TESTS")
def test_inline_google_font_css_can_inline_google_font_api_loader_css():
    url = ("%s/inline_google_font_css.html?PageSpeedFilters="
        "inline_google_font_css" % config.EXAMPLE_ROOT)

    userAgent = ("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/40.0.2214.45 Safari/537.36")

    headers = {"User-Agent" : userAgent}
    result, success = helpers.FetchUntil(url, headers = headers).waitFor(
        helpers.stringCountEquals, "@font-face", 7)
    assert success, result.body
    body = result.body

    lbody = body.lower()
    assert lbody.count("woff2") > 0
    assert lbody.count("format('truetype')") == 0
    assert lbody.count("embedded-opentype") == 0
    assert lbody.count(".ttf") == 0
    assert lbody.count(".eot") == 0

    # Now try with IE6 user-agent
    userAgent = "Mozilla/4.0 (compatible; MSIE 6.01; Windows NT 6.0)"
    headers = {"User-Agent" : userAgent}

    result, success = helpers.FetchUntil(url, headers = headers).waitFor(
        helpers.stringCountEquals, "@font-face", 1)
    assert success, result.body
    body = result.body
    lbody = body.lower()

    # This should get an eot font. (It might also ship a woff, so we don't
    # check_not_from for that)
    assert lbody.count(".eot") > 0
    assert lbody.count(".ttf") == 0


def test_inline_javascript_inlines_a_small_js_file():
    url = ("%s/inline_javascript.html?PageSpeedFilters=inline_javascript" %
        config.EXAMPLE_ROOT)
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "document.write", 1)
    assert success, result.body
