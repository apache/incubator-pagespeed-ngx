import config
import test_helpers as helpers

def test_elide_attributes_removes_boolean_and_default_attributes():
    filter_name = "elide_attributes"
    url = "%s/%s.html?PageSpeedFilters=%s" % (
        config.EXAMPLE_ROOT, filter_name, filter_name)
    assert helpers.fetch(url).body.count("disabled=") == 0
