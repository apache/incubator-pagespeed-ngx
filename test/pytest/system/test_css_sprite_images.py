import re

import config
from itertools import takewhile
import test_helpers as helpers


def test_inline_css_rewrite_css_sprite_images_sprites_images_in_css():
    page = ("sprite_images.html?PageSpeedFilters=inline_css,rewrite_css,"
        "sprite_images")
    url = "%s/%s" % (config.EXAMPLE_ROOT, page)
    pattern = r'Cuppa.png.*BikeCrashIcn.png.*IronChef2.gif.*.pagespeed.is.*.png'

    result, success = helpers.FetchUntil(url).waitFor(
        helpers.patternCountEquals, pattern, 1)
    assert success, result.body

def test_rewrite_css_sprite_images_sprites_images_in_css():
    page = "sprite_images.html?PageSpeedFilters=rewrite_css,sprite_images"
    url = "%s/%s" % (config.EXAMPLE_ROOT, page)
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "css.pagespeed.cf", 1)

    assert success, result

    # Extract out the rewritten CSS file from the HTML saved by FetchUntil
    # above (see -save and definition of FetchUntil).  Fetch that CSS
    # file and look inside for the sprited image reference (ic.pagespeed.is...).
    results = re.findall(
        r'[^"]*styles/A\.sprite_images\.css\.pagespeed\.cf\..*\.css',
        result.body)

    # If PreserveUrlRelativity is on, we need to find the relative URL and
    # absolutify it ourselves.
    results = [helpers.absolutify_url(url, u) for u in results]

    assert len(results) == 1
    css_url = results[0]

    print "css_url: %s" % css_url
    assert helpers.fetch(css_url).body.count("ic.pagespeed.is") > 0
