import argparse

from serial_reader import SerialReader
from serial_reader import SimulatedReader
from storage import Storage
from parser import parse_line

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("--port", help="COM port, e.g. COM4")
arg_parser.add_argument("--simulate", action="store_true", help="use simulated data instead of real hardware")

args = arg_parser.parse_args()

if args.simulate:
    reader = SimulatedReader()
elif args.port:
    reader = SerialReader(args.port)
else:
    print("Error: you must specify either --simulate or --port")
    exit()

store = Storage("environmental_monitoring_system.db")

try:
    while(1):
        line = reader.read_line()
        result = parse_line(line)
        if result is not None:
            store.append(result)
            print(result)
except KeyboardInterrupt:
    print("Key board interrupt!")
finally:
    store.close()
    if hasattr(reader, "close"):
        reader.close()