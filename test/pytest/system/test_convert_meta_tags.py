import config
import test_helpers as helpers


def test_convert_meta_tags():
    url = ("%s/convert_meta_tags.html?PageSpeedFilters=convert_meta_tags" %
       config.EXAMPLE_ROOT)
    resp, _body = helpers.fetch(url)
    content_type = resp.getheader("content-type")
    assert content_type
    assert content_type.count("text/html;") > 0
    assert content_type.count("charset=UTF-8") > 0
