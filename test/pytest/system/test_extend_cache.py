from datetime import datetime
import re

import config
import test_helpers as helpers


def test_extend_cache_images_rewrites_an_image_tag():
    url = ("%s/extend_cache.html?PageSpeedFilters=extend_cache_images" %
        config.EXAMPLE_ROOT)
    pattern = r'src.*/Puzzle[.]jpg[.]pagespeed[.]ce[.].*[.]jpg'
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.patternCountEquals, pattern, 1)
    assert success, result.body
    # echo about to test resource ext corruption...
    # test_resource_ext_corruption $URL
    # images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg


def test_attempt_to_fetch_cache_extended_image_without_hash_should_404():
    url = "%s/images/Puzzle.jpg.pagespeed.ce..jpg" % config.REWRITTEN_ROOT
    assert helpers.fetch(url, allow_error_responses = True).resp.status == 404


def test_cache_extended_image_should_respond_304_to_an_if_modified_since():
    url = ("%s/images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg" %
        config.REWRITTEN_ROOT)
    now = helpers.http_date(datetime.now())
    result = helpers.fetch(url, {"If-Modified-Since": now})
    assert result.resp.status == 304


def test_legacy_format_urls_should_still_work():
    url = ("%s/images/ce.0123456789abcdef0123456789abcdef.Puzzle,j.jpg" %
        config.REWRITTEN_ROOT)
    helpers.fetch(url)

# Cache extend PDFs.


def test_extend_cache_pdfs_pdf_cache_extension():
    url = ("%s/extend_cache_pdfs.html?PageSpeedFilters=extend_cache_pdfs" %
        config.EXAMPLE_ROOT)
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, ".pagespeed.", 3)
    assert success, result.body
    body = result.body

    assert len(re.findall("a href=\".*pagespeed.*\.pdf", body)) > 0
    assert len(re.findall("embed src=\".*pagespeed.*\.pdf", body)) > 0
    assert len(re.findall("<a href=\"example.notpdf\">", body)) > 0
    assert len(
        re.findall(
            "<a href=\".*pagespeed.*\\.pdf\">example.pdf\\?a=b",
            body)) > 0


def test_cache_extended_pdfs_load_and_have_the_right_mime_type():
    url = ("%s/extend_cache_pdfs.html?PageSpeedFilters=extend_cache_pdfs" %
        config.EXAMPLE_ROOT)

    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, ".pagespeed.", 3)
    assert success, result.body
    body = result.body

    results = re.findall(r'http://[^\"]*pagespeed.[^\"]*\.pdf', body)

    if len(results) == 0:
        # If PreserveUrlRelativity is on, we need to find the relative URL and
        # absolutify it ourselves.
        results = re.findall(r'[^\"]*pagespeed.[^\"]*\.pdf', body)
        results = [helpers.absolutify_url(url, u) for u in results]

    assert len(results) > 0

    resp, body = helpers.fetch(results[1])
    assert resp.getheader("content-type") == "application/pdf"
