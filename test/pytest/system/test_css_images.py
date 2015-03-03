import config
import test_helpers as helpers


def test_rewrite_css_extend_cache_extends_cache_of_images_in_css():
    page = "rewrite_css_images.html?PageSpeedFilters=rewrite_css,extend_cache"
    url = "%s/%s" % (config.EXAMPLE_ROOT, page)

    # image cache extended
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "Cuppa.png.pagespeed.ce.", 1)
    assert success, result.body
