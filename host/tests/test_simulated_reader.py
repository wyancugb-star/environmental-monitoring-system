import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from parser import parse_line
from serial_reader import SimulatedReader

def test_simulated_output():
    reader = SimulatedReader(interval_s=0.01, inject_errors=False)
    for i in range(20):
        line = reader.read_line()
        result = parse_line(line)
        assert result is not None

def test_simulated_output_error_injection():
    reader = SimulatedReader(initial_temp=85.0,interval_s=0.01, inject_errors=True)

    parsed_count = 0
    skipped_count = 0
    temp_err_count = 0
    light_err_count = 0

    for i in range(200):   
        line = reader.read_line()
        print(repr(line))
        result = parse_line(line)
        print(result)
        if result is None:
            skipped_count += 1
        else:
            parsed_count += 1
            if result.temp_err:
                temp_err_count += 1
            if result.light_err:
                light_err_count += 1

    assert skipped_count > 0
    assert temp_err_count > 0
    assert light_err_count > 0
    assert skipped_count + parsed_count == 200