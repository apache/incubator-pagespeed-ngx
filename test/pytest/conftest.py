import pytest
from config import FETCH_ON_START_AND_END
from config import log
from test_helpers import fetch

def pytest_addoption(parser):
    parser.addoption(
        '--count',
        default=1,
        type='int',
        metavar='count',
        help='Run each test the specified number of times')

def pytest_generate_tests(metafunc):
    for _i in range(metafunc.config.option.count):
        metafunc.addcall()

@pytest.fixture(scope="function")
def log_test_start(request):
    log.debug("[%s] start" % request.function.__name__)
    if FETCH_ON_START_AND_END:
        # Mark test start in nginx.log
         fetch("/psol_test_start_%s" %  request.function.__name__,
            method = "HEAD", allow_error_responses = True)
    def log_test_end():
        if FETCH_ON_START_AND_END:
            # Mark test end in nginx.log
            log.debug("[%s] end\n\n" % request.function.__name__)
            fetch("/psol_test_end_%s" % request.function.__name__,
                method = "HEAD", allow_error_responses = True)

    request.addfinalizer(log_test_end)
