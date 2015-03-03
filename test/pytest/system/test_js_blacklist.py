import re

import config
import test_helpers as helpers


def test_filters_do_not_rewrite_blacklisted_javascript_files():
    url = ("%s/blacklist/blacklist.html?PageSpeedFilters="
        "extend_cache,rewrite_javascript,trim_urls" % config.TEST_ROOT)
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, ".js.pagespeed.", 4)
    assert success, result.body
    body = result.body

    assert len(
        re.findall(
            r'<script src=\".*normal\.js\.pagespeed\..*\.js\">',
            body)) > 0
    assert len(
        re.findall(
            r'<script src=\"js_tinyMCE\.js\"></script>',
            body)) > 0
    assert len(re.findall(r'<script src=\"tiny_mce\.js\"></script>', body)) > 0
    assert len(re.findall(r'<script src=\"tinymce\.js\"></script>', body)) > 0
    q = r'<script src=\"scriptaculous\.js\?load=effects,builder\"></script>'
    assert len(re.findall(q, body)) > 0
    q = r'<script src=\".*jquery.*\.js\.pagespeed\..*\.js\">'
    assert len(re.findall(q, body)) > 0
    assert len(re.findall(r'<script src=\".*ckeditor\.js\">', body)) > 0
    assert len(
        re.findall(
            r'<script src=\".*swfobject\.js\.pagespeed\..*\.js\">',
            body)) > 0
    assert len(
        re.findall(
            r'<script src=\".*another_normal\.js\.pagespeed\..*\.js\">',
            body)) > 0
