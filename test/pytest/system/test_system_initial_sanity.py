import config
import test_helpers as helpers
# This tests whether fetching "/" gets you "/index.html".  With async
# rewriting, it is not deterministic whether inline css gets
# rewritten.  That's not what this is trying to test, so we use
# ?PageSpeed=off.

def test_directory_mapped_to_index():
    _resp, autoIndexBody = helpers.fetch(
        "%s/?PageSpeed=off" %
        config.EXAMPLE_ROOT)
    _resp, indexBody = helpers.fetch(
        "%s/index.html?PageSpeed=off" %
        config.EXAMPLE_ROOT)
    assert indexBody == autoIndexBody


def test_compression_enabled_for_html():
    resp, _body = helpers.fetch(
        "%s/" %
        config.EXAMPLE_ROOT, headers={
            'Accept-Encoding': 'gzip'})
    assert resp.getheader("content-encoding") == "gzip"


def test_whitespace_served_as_html_behaves_sanely():
    helpers.fetch("%s/whitespace.html" % config.TEST_ROOT)
