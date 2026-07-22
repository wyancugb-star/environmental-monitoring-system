import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from storage import Storage
from parser import Reading

def test_append_then_query():
    db_path = "test_storage.db"
    if os.path.exists(db_path):
        os.remove(db_path)
        
    store = Storage(db_path)
    store.append(Reading("2026-07-16", "14:25:38", 25.3, False, 67, False))
    result = store.query_range("2026-07-16 00:00:00", "2026-07-16 23:59:59")
    store.close()

    assert len(result) == 1
    assert result[0].date == "2026-07-16"
    assert result[0].time == "14:25:38"
    assert result[0].temp_c == 25.3
    assert result[0].temp_err == False
    assert result[0].light_pct == 67
    assert result[0].light_err == False

def test_query_range():
    db_path = "test_storage.db"
    if os.path.exists(db_path):
        os.remove(db_path)

    store = Storage(db_path)
    store.append(Reading("2026-07-16", "14:00:00", 25.3, False, 67, False))
    store.append(Reading("2026-07-16", "14:05:00", 25.3, False, 67, False))
    store.append(Reading("2026-07-16", "14:10:00", 25.3, False, 67, False))
    result = store.query_range("2026-07-16 14:02:00", "2026-07-16 14:08:00")
    store.close()

    assert len(result) == 1
    assert result[0].date == "2026-07-16"
    assert result[0].time == "14:05:00"
    assert result[0].temp_c == 25.3
    assert result[0].temp_err == False
    assert result[0].light_pct == 67
    assert result[0].light_err == False

def test_persistence_across_restart():
    db_path = "test_storage.db"
    if os.path.exists(db_path):
        os.remove(db_path)

    store1 = Storage(db_path)
    store1.append(Reading("2026-07-16", "14:05:00", 25.3, False, 67, False))
    store1.close() 

    store2 = Storage(db_path)  
    result = store2.query_range("2026-07-16 14:02:00", "2026-07-16 14:08:00")
    store2.close()

    assert len(result) == 1
    assert result[0].date == "2026-07-16"
    assert result[0].time == "14:05:00"
    assert result[0].temp_c == 25.3
    assert result[0].temp_err == False
    assert result[0].light_pct == 67
    assert result[0].light_err == False