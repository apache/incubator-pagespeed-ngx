import config
import test_helpers as helpers



def test_rewrite_css_rewrite_images_rewrites_and_inlines_images_in_css():
    page = ("rewrite_css_images.html?PageSpeedFilters="
        "rewrite_css,rewrite_images&ModPagespeedCssImageInlineMaxBytes=2048")
    url = "%s/%s" % (config.EXAMPLE_ROOT, page)

    # image inlined
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "data:image/png;base64", 1)
    assert success, result.body
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "rewrite_css_images.css.pagespeed.cf.", 1)
    assert success, result.body
