import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import pytest
from serial_reader import SerialReader
from parser import parse_line

@pytest.mark.hardware
def test_real_device_output_parses_correctly(port):
    if port is None:
        pytest.skip("No --port provided, skipping hardware test")

    reader = SerialReader(port, baudrate=115200, timeout=3.0)

    parsed_count = 0
    for i in range(10):
        line = reader.read_line()
        if line == "":
            continue
        result = parse_line(line)
        if result is not None:
            parsed_count += 1

    reader.close()
    assert parsed_count > 0