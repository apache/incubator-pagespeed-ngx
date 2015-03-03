import config
import test_helpers as helpers


def test_rewrite_images_fails_broken_image():
    url = ("%s/images/xBadName.jpg.pagespeed.ic.Zi7KMNYwzD.jpg"
        % config.REWRITTEN_ROOT)
    assert helpers.fetch(url, allow_error_responses = True).resp.status == 404


def test_rewrite_images_does_not_500_on_unoptomizable_image():
    url = ("%s/images/xOptPuzzle.jpg.pagespeed.ic.Zi7KMNYwzD.jpg"
        % config.REWRITTEN_ROOT)
    helpers.fetch(url)
