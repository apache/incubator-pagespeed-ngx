import config
import test_helpers as helpers




def test_move_css_above_scripts_works():
    url = ("%s/move_css_above_scripts.html?PageSpeedFilters="
        "move_css_above_scripts" % config.EXAMPLE_ROOT)
    _resp, body = helpers.fetch(url)
    # Link moved before script.
    assert body.count("styles/all_styles.css\"><script") > 0


def test_move_css_above_scripts_off():
    url = ("%s/move_css_above_scripts.html?PageSpeedFilters="
        % config.EXAMPLE_ROOT)
    _resp, body = helpers.fetch(url)
    # Link not moved before script.
    assert body.count("styles/all_styles.css\"><script") == 0


def test_move_css_to_head_does_what_it_says_on_the_tin():
    url = ("%s/move_css_to_head.html?PageSpeedFilters=move_css_to_head"
        % config.EXAMPLE_ROOT)
    _resp, body = helpers.fetch(url)
    # Link moved to head.
    assert body.count("styles/all_styles.css\"></head>") > 0


def test_move_css_to_head_off():
    url = "%s/move_css_to_head.html?PageSpeedFilters=" % config.EXAMPLE_ROOT
    _resp, body = helpers.fetch(url)
    # Link moved to head.
    assert body.count("styles/all_styles.css\"></head>") == 0
