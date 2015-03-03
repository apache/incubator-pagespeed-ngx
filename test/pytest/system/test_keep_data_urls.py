import config
import test_helpers as helpers


# Make sure we don't blank url(data:...) in CSS.
def test_css_data_urls():
    url  = "%s/styles/A.data.css.pagespeed.cf.Hash.css" % config.REWRITTEN_ROOT
    result = helpers.fetch(url)
    assert result.body.count("data:image/png"), result.body
