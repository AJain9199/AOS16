import serial
import time

def read8(srl):
    return int.from_bytes(srl.read(1))

def read32(srl):
    return (read8(srl) << 24) | (read8(srl) << 16) | (read8(srl) << 8) | read8(srl) 

def manufacturer_id_handshake(srl):
    srl.write('c'.encode())
    time.sleep(0.5)
    manufacturer_id = read8(srl)
    chip_id = read8(srl)

    if manufacturer_id == 0xBF and chip_id == 0xB5:
        print("Manufacturer handshake successful")
    else:
        return

def chip_erase(srl):
    srl.write('e'.encode())
    time.sleep(0.5)
    if read8(srl) == 0xff:
        print("Chip erased.")
    else:
        print("Chip erase failed.")

def write32(srl, n):
    srl.write(bytearray([(n >> 24) & 0xff, (n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff]))

def write8(srl, n):
    srl.write(n)

def write_addr(srl, data):
    srl.write('w'.encode())
    srl.write(len(data).to_bytes(4, 'big'))
    srl.write('a'.encode())

    for (addr, byte) in data:
        print(byte)
        srl.write(addr.to_bytes(4, 'big'))
        srl.write(byte.to_bytes(1))
        time.sleep(0.5)

        print(srl.readline())
        print(srl.readline())
    
    if (read32(srl) == len(data)):
        print("Write successful")

def read_addr(srl, addrs):
    srl.write('r'.encode())
    write32(srl, len(addrs))
    print(srl.readline())
    srl.write('a'.encode())
    
    data = []

    for i in addrs:
        write32(srl, i);
    
    for i in addrs:
        data.append(read8(srl))
    return data

def main():
    srl = serial.Serial('COM10')
    srl.baudrate = 115200  # set Baud rate to 9600
    srl.bytesize = 8   # Number of data bits = 8
    srl.parity  ='N'   # No parity
    srl.stopbits = 1   # Number of Stop bits = 1

    time.sleep(1)
    manufacturer_id_handshake(srl)

    chip_erase(srl);

    write_addr(srl, [(2, 12)])
    print(read_addr(srl, [2]))
    

if __name__ == "__main__":
    main()