"""
storage.py
Local persistence for parsed Reading objects, backed by SQLite.
"""
import sqlite3
from parser import Reading

class Storage:
    """
    append parsed readings to a durable local store for later analysis
    """
    def __init__(self, db_path):
        """
        save file path
        if there is no file then create DB file
        """
        self.conn = sqlite3.connect(db_path)
        self.conn.execute("CREATE TABLE IF NOT EXISTS readings (timestamp TEXT, date TEXT, time TEXT, temp_c REAL, temp_err INTEGER, light_pct INTEGER, light_err INTEGER)")
        self.conn.commit()

    def append(self, reading:Reading):
        """
        add a rwo of data to DB file
        """
        self.conn.execute("INSERT INTO readings (timestamp, date, time, temp_c, temp_err, light_pct, light_err) VALUES (?, ?, ?, ?, ?, ?, ?)",
     (reading.date+" " +reading.time, reading.date, reading.time, reading.temp_c, reading.temp_err, reading.light_pct, reading.light_err))
        self.conn.commit()

    def query_range(self,start,end):
        """
        Return all readings with a timestamp between start and end (inclusive).

        start, end: strings in "YYYY-MM-DD HH:MM:SS" format (same format as
        reading.date + " " + reading.time), e.g. "2026-07-16 14:25:38".
        Passing a different format (e.g. day/month/year) won't raise an
        error -- it will just silently return wrong or empty results, since
        the comparison is a plain string comparison, not a real date/time
        comparison.

        Returns a list of Reading objects, ordered as stored in the database
        (not guaranteed to be sorted by time unless you query them in insertion order).
        """
        rows = self.conn.execute(
        "SELECT date, time, temp_c, temp_err, light_pct, light_err FROM readings WHERE timestamp >= ? AND timestamp <= ?",(start, end)).fetchall()
        result = []
        for row in rows:
            reading = Reading(
            date=row[0],
            time=row[1],
            temp_c=row[2],
            temp_err=bool(row[3]),
            light_pct=row[4],
            light_err=bool(row[5]))
            result.append(reading)
        return result
    
    def close(self):
        self.conn.close()

