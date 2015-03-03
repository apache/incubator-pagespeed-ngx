import config
import test_helpers as helpers



def test_filter_rewrite_css_minifies_css_and_saves_bytes():
    url = ("%s/rewrite_css.html?PageSpeedFilters=rewrite_css" %
      config.EXAMPLE_ROOT)
    _resp, body = helpers.fetch(url)
    assert body.count("comment") == 0
    assert len(body) < 680  # down from 689
