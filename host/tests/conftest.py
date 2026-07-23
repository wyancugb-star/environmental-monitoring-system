import pytest

def pytest_addoption(parser):
    parser.addoption("--port", action="store", default=None, help="COM port for hardware tests")

@pytest.fixture
def port(request):
    return request.config.getoption("--port")