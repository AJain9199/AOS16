import serial
import time
SerialObj = serial.Serial('COM9')
SerialObj.baudrate = 9600  # set Baud rate to 9600
SerialObj.bytesize = 8   # Number of data bits = 8
SerialObj.parity  ='N'   # No parity
SerialObj.stopbits = 1   # Number of Stop bits = 1
time.sleep(3)

data = SerialObj.read(1);
print(int.from_bytes(data));
data = SerialObj.read(1);
print(int.from_bytes(data));
