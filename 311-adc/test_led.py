
import time
import serial
from serial.tools import list_ports

port_name = None

for port in list_ports.comports():
    if port.vid == 0x2E8A and port.pid == 0x000A:
        port_name = port.device
        break

if port_name is None:
    print("Raspberry Pi Pico не найдена")
    raise SystemExit(1)

print("Плата найдена:", port_name)

commands = [
    "led_enable",
    "led_disable",
    "led_period 2000",
    "led_blink",
    "led_period 0",
]

with serial.Serial(port_name, 115200, timeout=0.1) as pico:
    time.sleep(1)

    for command in commands:
        print("\nКоманда:", command)

        pico.reset_input_buffer()
        pico.write((command + "\r").encode("ascii"))

        deadline = time.monotonic() + 0.7

        while time.monotonic() < deadline:
            data = pico.read(256)
            if data:
                print(data.decode("utf-8", errors="replace"), end="")

print("\nПроверка завершена")
