import re

import config
import test_helpers as helpers



def test_rewrite_javascript_inline_javascript_with_gzipped_js_origin():
    print """Testing whether we can rewrite javascript resources that are served
gzipped, even though we generally ask for them clear.  This particular
js file has "alert('Hello')" but is checked into source control in gzipped
format and served with the gzip headers, so it is decodable.  This tests
that we can inline and minify that file.
"""

    url = ("%s/rewrite_compressed_js.html?PageSpeedFilters=rewrite_javascript,"
        "inline_javascript" % config.TEST_ROOT)

    result, success = helpers.FetchUntil(url).waitFor(
        helpers.patternCountEquals, "Hello'", 1)
    assert success, result.body

