import random
import serial
import time

from datetime import datetime, timedelta

class SerialReader:
    
    def __init__(self, port="COM1", baudrate=115200, timeout=2.0):
        """
        Open a connection to a real serial port.
        port: e.g. "COM1". baudrate: e.g. 115200. timeout: seconds "".
        """
        self._ser = serial.Serial(port, baudrate, timeout=timeout)

    def read_line(self):
        """Blocks until a line is available or the timeout elapses (returns '' on timeout)."""
        raw = self._ser.readline()
        return raw.decode("ascii", errors="replace")
    
    def write_line(self, text):
        """
        Send a text command to the real device over UART, e.g. for the
        GUI-issued SET_TIME: command (requirements_spec.md §6.4).
        """
        self._ser.write(text.encode("ascii"))
    
    def close(self):
        self._ser.close()

class SimulatedReader:
    """Generates lines for development without hardware."""
    def __init__(self,initial_temp=25.3,interval_s=1.0,inject_errors=False):
        """
        initial_temp: initial temperature
        interval_s: time interval between two messages
        inject_errors:  inject error message
        """
        self.dt = datetime(2026, 7, 22, 17, 34, 59)
        self.temp = initial_temp
        self.light = 68
        self.interval_s=interval_s
        self.inject_errors = inject_errors

    def read_line(self):
        """
        Returns one simulated line per requirements_spec.md §6.1.

        Non-data lines (usmart echoes): only if inject_errors=True, ~10% chance.
        TEMP/LIGHT _ERR: independent of inject_errors -- values drift randomly
        each call, and get clamped + flagged "_ERR" if they exceed
        [-40.0, 85.0] (temp) or [0, 100] (light).
        """
        time.sleep(self.interval_s)

        if self.inject_errors and random.random() < 0.1:
            return "RTC SET OK: 2026-07-16 14:00:00\r\n"

        self.dt += timedelta(seconds=1)
        date_str = self.dt.strftime("%Y-%m-%d")
        time_str = self.dt.strftime("%H:%M:%S")
        
        self.temp += random.uniform(-0.3, 0.3)
        self.light += random.randint(-5, 5)

        temp_err_suffix = ""
        light_err_suffix = ""
        
        if self.temp <-40.0:
            self.temp = -40.0
            temp_err_suffix = "_ERR"
        elif self.temp > 85.0:
            self.temp = 85.0
            temp_err_suffix = "_ERR"

        if self.light<0:
            self.light = 0
            light_err_suffix = "_ERR"
        elif self.light > 100:
            self.light = 100
            light_err_suffix = "_ERR"

        return f"DATE:{date_str},TIME:{time_str},TEMP:{self.temp:.1f}{temp_err_suffix},LIGHT:{self.light}{light_err_suffix}"
    
    def write_line(self, text):
        """
        Simulated mode has no real device to send to -- just print what
        would have been sent, so the GUI's Set RTC Time button still gives
        visible feedback during development/testing without hardware.
        """
        print(f"[SimulatedReader] would send: {text!r}")

    
