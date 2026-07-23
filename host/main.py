from serial_reader import SimulatedReader, SerialReader
from parser import parse_line
from storage import Storage

from PyQt5.QtCore import QThread, pyqtSignal, QDateTime, Qt
from PyQt5.QtWidgets import QWidget, QApplication, QLabel, QVBoxLayout, QProgressBar, QHBoxLayout, QComboBox, QPushButton, QLineEdit, QDateTimeEdit

from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

from collections import deque
from datetime import datetime, timedelta

import sys

def temp_to_bar_pct(temp_c):
        min_c = -10
        max_c = 55
        if temp_c <= min_c:
            return 0
        if temp_c >= max_c:
            return 100
        return 100*(temp_c-min_c)/(max_c-min_c)

class ReaderThread(QThread):
    new_reading = pyqtSignal(object)   

    def __init__(self, reader):
        super().__init__()

        self.reader = reader
        self.running = True

    def run(self):
        while self.running:
            line = self.reader.read_line()
            result = parse_line(line)
            if result is not None:
                self.new_reading.emit(result)   

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()

        self.store = Storage("environmental_monitoring_system.db")

        self.setWindowTitle("Environmental Monitor")
        self.resize(600, 800)

        self.datetime_picker = QDateTimeEdit()

        self.set_time_button = QPushButton("Set RTC Time")
        self.set_time_button.setEnabled(False)

        rtc_time_row = QHBoxLayout()
        rtc_time_row.addWidget(self.datetime_picker)
        rtc_time_row.addWidget(self.set_time_button)

        self.mode_combo = QComboBox()
        self.mode_combo.addItem("Simulate")
        self.mode_combo.addItem("Real Hardware")

        self.port_input = QLineEdit()
        self.port_input.setPlaceholderText("COM port, e.g. COM4")
        self.start_button = QPushButton("Start")

        self.stop_button = QPushButton("Stop")
        self.stop_button.setEnabled(False)  
        
        setting_row=QHBoxLayout()
        setting_row.addWidget(self.mode_combo)
        setting_row.addWidget(self.port_input)
        setting_row.addWidget(self.start_button)
        setting_row.addWidget(self.stop_button)

        self.date_label = QLabel("DATE: --")
        self.time_label = QLabel("TIME: --")
        self.temp_label = QLabel("TEMP: --")
        self.light_label = QLabel("LIGHT: --")

        self.date_label.setAlignment(Qt.AlignCenter)
        self.time_label.setAlignment(Qt.AlignCenter)

        datatime_row=QHBoxLayout()
        datatime_row.addWidget(self.date_label)
        datatime_row.addWidget(self.time_label)

        datatime_row.setAlignment(Qt.AlignCenter)

        self.temp_bar = QProgressBar()
        self.temp_bar.setMinimum(0)
        self.temp_bar.setMaximum(100)

        temp_row = QHBoxLayout()
        temp_row.addWidget(self.temp_label)
        temp_row.addWidget(self.temp_bar)

        self.light_bar = QProgressBar()
        self.light_bar.setMinimum(0)
        self.light_bar.setMaximum(100)
        
        light_row = QHBoxLayout()
        light_row.addWidget(self.light_label)
        light_row.addWidget(self.light_bar)

        self.last_1min_button = QPushButton("Last 1 min")
        self.last_10min_button = QPushButton("Last 10 min")
        self.last_1hour_button = QPushButton("Last 1 hour")

        time_range_row = QHBoxLayout()
        time_range_row.addWidget(self.last_1min_button)
        time_range_row.addWidget(self.last_10min_button)
        time_range_row.addWidget(self.last_1hour_button)

        self.temp_figure = Figure()
        self.temp_canvas = FigureCanvas(self.temp_figure)
        self.temp_ax = self.temp_figure.add_subplot(111) 
        self.temp_history = deque(maxlen=20)

        self.light_figure = Figure()
        self.light_canvas = FigureCanvas(self.light_figure)
        self.light_ax = self.light_figure.add_subplot(111) 
        self.light_history = deque(maxlen=20)

        self.x_history = deque(maxlen=20)
        self.point_count = 0   

        self.time_history = deque(maxlen=20)

        layout = QVBoxLayout()
        layout.addLayout(rtc_time_row)
        layout.addLayout(setting_row)
        layout.addLayout(datatime_row)
        layout.addLayout(temp_row)
        layout.addLayout(light_row)
        layout.addLayout(time_range_row)
        layout.addWidget(self.temp_canvas)
        layout.addWidget(self.light_canvas)
        layout.addStretch()

        self.setLayout(layout)

        self.start_button.clicked.connect(self.on_start_clicked) 
        self.stop_button.clicked.connect(self.on_stop_clicked) 
        self.last_1min_button.clicked.connect(self.on_last_1min_clicked) 
        self.last_10min_button.clicked.connect(self.on_last_10min_clicked) 
        self.last_1hour_button.clicked.connect(self.on_last_1hour_clicked) 
        self.set_time_button.clicked.connect(self.on_set_time_clicked)

    def on_start_clicked(self):
        mode = self.mode_combo.currentText()

        if mode == "Simulate":
            reader = SimulatedReader(interval_s=1, inject_errors=True)
        else:
            port = self.port_input.text()
            reader = SerialReader(port)

        self.reader_thread = ReaderThread(reader)
        self.reader_thread.new_reading.connect(self.on_new_reading)
        self.reader_thread.start()

        self.mode_combo.setEnabled(False)
        self.port_input.setEnabled(False)
        self.start_button.setEnabled(False)
        self.stop_button.setEnabled(True)
        self.last_1min_button.setEnabled(False)
        self.last_10min_button.setEnabled(False)
        self.last_1hour_button.setEnabled(False)
        self.set_time_button.setEnabled(True)

    def on_stop_clicked(self):
        self.reader_thread.running = False

        if hasattr(self.reader_thread.reader, "close"):
            self.reader_thread.reader.close()

        self.mode_combo.setEnabled(True)
        self.port_input.setEnabled(True)
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.last_1min_button.setEnabled(True)
        self.last_10min_button.setEnabled(True)
        self.last_1hour_button.setEnabled(True)
        self.set_time_button.setEnabled(False)

    def show_history(self, minutes):
        now = datetime.now()
        start = (now - timedelta(minutes=minutes)).strftime("%Y-%m-%d %H:%M:%S")
        end = now.strftime("%Y-%m-%d %H:%M:%S")
        results = self.store.query_range(start, end)

        times = []
        temps = []
        lights = []

        for r in results:
            times.append(r.time)
            temps.append(r.temp_c)
            lights.append(r.light_pct)

        x_positions = list(range(len(results)))

        self.temp_ax.clear()   
        self.temp_ax.set_ylim(-10, 50)     
        self.temp_ax.set_title("TEMP")
        self.temp_ax.set_xlabel("time")
        self.temp_ax.set_ylabel("Celsius(°C)")
        self.temp_ax.plot(x_positions, temps)

        self.light_ax.clear()   
        self.light_ax.set_ylim(0, 100)   
        self.light_ax.set_title("LIGHT")
        self.light_ax.set_xlabel("time")
        self.light_ax.set_ylabel("brightness(%)")  
        self.light_ax.plot(x_positions, lights)

        target_labels = 5  
        step = max(1, int(len(results)/target_labels))

        tick_positions = list(x_positions)[::step]
        tick_labels = list(times)[::step]
        self.temp_ax.set_xticks(tick_positions)
        self.temp_ax.set_xticklabels(tick_labels)
        self.light_ax.set_xticks(tick_positions)
        self.light_ax.set_xticklabels(tick_labels)

        self.temp_canvas.draw()   
        self.light_canvas.draw()


    def on_last_1min_clicked(self):
        self.show_history(1)

    def on_last_10min_clicked(self):
        self.show_history(10)

    def on_last_1hour_clicked(self):
        self.show_history(60)

    def on_set_time_clicked(self):
        selected = self.datetime_picker.dateTime()
        date_str = selected.toString("yyyy-MM-dd")
        time_str = selected.toString("HH:mm:ss")
        command = f"SET_TIME:{date_str},{time_str}\n"
        self.reader_thread.reader.write_line(command)

    def on_new_reading(self, reading):
        self.store.append(reading)

        self.datetime_picker.setDateTime(QDateTime.currentDateTime())

        self.date_label.setText(f"DATE:{reading.date}")

        self.time_label.setText(f"TIME:{reading.time}")

        self.temp_label.setText(f"TEMP:{reading.temp_c:.1f}")
        self.temp_bar.setValue(int(temp_to_bar_pct(reading.temp_c)))

        if reading.temp_err:
            self.temp_label.setStyleSheet("color: red;")
        else:
            self.temp_label.setStyleSheet("color: black;")

        self.light_label.setText(f"LIGHT:{reading.light_pct}")
        self.light_bar.setValue(reading.light_pct)

        if reading.light_err:
            self.light_label.setStyleSheet("color: red;")
        else:
            self.light_label.setStyleSheet("color: black;")

        self.temp_history.append(reading.temp_c)
        self.light_history.append(reading.light_pct)
        self.x_history.append(self.point_count)
        self.time_history.append(reading.time)
        self.point_count += 1

        self.temp_ax.clear()   
        self.temp_ax.set_ylim(-10, 55)     
        self.temp_ax.set_title("TEMP")
        self.temp_ax.set_xlabel("time")
        self.temp_ax.set_ylabel("Celsius(°C)")
        self.temp_ax.plot(self.x_history, self.temp_history)

        self.light_ax.clear()   
        self.light_ax.set_ylim(0, 100)   
        self.light_ax.set_title("LIGHT")
        self.light_ax.set_xlabel("time")
        self.light_ax.set_ylabel("brightness(%)")  
        self.light_ax.plot(self.x_history, self.light_history)

        self.temp_figure.tight_layout()
        self.light_figure.tight_layout()

        step = 5
        tick_positions = list(self.x_history)[::step]
        tick_labels = list(self.time_history)[::step]
        self.temp_ax.set_xticks(tick_positions)
        self.temp_ax.set_xticklabels(tick_labels)
        self.light_ax.set_xticks(tick_positions)
        self.light_ax.set_xticklabels(tick_labels)

        self.temp_canvas.draw()   
        self.light_canvas.draw()  

app = QApplication(sys.argv)
window = MainWindow()
window.show()
sys.exit(app.exec_())
