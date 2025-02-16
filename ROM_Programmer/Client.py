import serial
import time

srl = serial.Serial('COM9')
srl.baudrate = 9600  # set Baud rate to 9600
srl.bytesize = 8   # Number of data bits = 8
srl.parity  ='N'   # No parity
srl.stopbits = 1   # Number of Stop bits = 1

def writeData(addr, data): 
    srl.write(bytearray([addr >> 8, addr & 0xff, data]))
    time.sleep(0.001)
    return int.from_bytes(srl.read(1)) == data
    
def writeBlock(data):
    srl.write('w'.encode())
    srl.write(bytearray([(data >> 8), data & 0xff]))
    time.sleep(0.1)
    for (addr, d) in data:
        writeData(addr, d)

time.sleep(3)

SerialObj.write('c'.encode());

time.sleep(0.1);
manufacturer_id = int.from_bytes(SerialObj.read(1));
chip = int.from_bytes(SerialObj.read(1));

if manufacturer_id == 0xBF and chip == 0xB5:
    print("Manufacturer handshake successful.")

print("Initiating Chip Erase.")
SerialObj.write('e'.encode())
time.sleep(10);
response = int.from_bytes(SerialObj.read(1));
if response == 0xFF:
    print("Chip Erase Successful.")

SerialObj.write('w'.encode());
SerialObj.write(bytearray([0, 2]));
print(SerialObj.readline());

SerialObj.write([0, 0, 1]);
SerialObj.write([0, 1, 2]);

time.sleep(1)

print(int.from_bytes(SerialObj.read(1)));

time.sleep(1);

SerialObj.write('r'.encode())
time.sleep(0.1);
print(int.from_bytes(SerialObj.read(1)));
