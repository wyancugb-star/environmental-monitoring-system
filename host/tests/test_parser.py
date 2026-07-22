import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from parser import parse_line

def test_well_formed_frame_parses_correctly():
    reading = parse_line("DATE:2026-07-16,TIME:14:25:38,TEMP:25.3,LIGHT:67")
    assert reading.date=="2026-07-16"
    assert reading.time=="14:25:38"
    assert reading.temp_c==25.3
    assert reading.temp_err==False
    assert reading.light_pct==67
    assert reading.light_err==False

def test_parsed_error_flag():
    reading = parse_line("DATE:2026-07-16,TIME:14:25:38,TEMP:85.0_ERR,LIGHT:100_ERR")
    assert reading.date=="2026-07-16"
    assert reading.time=="14:25:38"
    assert reading.temp_c==85.0
    assert reading.temp_err==True
    assert reading.light_pct==100
    assert reading.light_err==True

def test_non_data_lines_skipped():
    reading1 = parse_line("RTC SET OK: 2026-07-16 14:00:00")
    reading2 = parse_line("")
    reading3 = parse_line("usmart>")
    assert all((reading1==None,reading2==None,reading3==None))

def test_malformed_data_skipped():
    reading = parse_line("DATE:2026-07-16,TIME:14:25:38,TEMP:abc,LIGHT:67")
    assert reading==None