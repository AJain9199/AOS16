import serial
import time
import sys
import argparse
from argparse import ArgumentParser
from tqdm import tqdm


CHUNK_SIZE = 4096

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
    srl.write(n.to_bytes(4, 'big'))


def write8(srl, n):
    srl.write(n)


def write_data(srl, data):
    srl.write('w'.encode())
    srl.write(len(data).to_bytes(4, 'big'))
    srl.write('a'.encode())

    bytedata = []

    for (addr, byte) in data:
        bytedata.extend(addr.to_bytes(4, 'big'))
        bytedata.extend(byte.to_bytes(1))

    srl.write(bytedata)

    return read32(srl) == len(data)


def write_seq_chunk(srl, b, nb=0):
    srl.write('w'.encode())
    srl.write(len(b).to_bytes(4, 'big'))
    srl.write('s'.encode())

    srl.write(nb.to_bytes(4, 'big'))

    time.sleep(1)

    srl.write(b)

    length = int.from_bytes(srl.read(4), 'big')
    return length == len(b)


def write_seq(srl, f):
    nbytes = 0
    full_seq = []
    nchunk = 0
    while True:
        chunk = f.read(CHUNK_SIZE)
        if not chunk or len(chunk) == 0:
            break
        write_seq_chunk(srl, chunk, nbytes)

        print(f"Chunk {nchunk} with {len(chunk)} bytes written.")
        full_seq.append(chunk)
        nbytes += len(chunk)
        nchunk += 1

    print("Write complete. Verification begun.")
    for i in range(len(full_seq)):
        if chunk != read_seq(srl, len(chunk), CHUNK_SIZE*i):
            print(f"Chunk {i} is compromised.")
        else:
            print(f"Chunk {i} is good.")
    print("Verification complete.")
    return nbytes


def read_addr(srl, addrs):
    srl.write('r'.encode())
    write32(srl, len(addrs))
    srl.write('a'.encode())

    bytedata = []

    for i in addrs:
        bytedata.extend(i.to_bytes(4, 'big'))

    srl.write(bytedata)

    return list(srl.read(len(addrs)))


def read_seq(srl, n, start=0):
    srl.write('r'.encode())
    srl.write(n.to_bytes(4, 'big'))
    srl.write('s'.encode())

    srl.write(start.to_bytes(4, 'big'))

    srl.reset_input_buffer()
    srl.reset_output_buffer()

    time.sleep(0.5)
    return srl.read(n)


def parse_paired(filename):
    return [()]


def parse_single(filename):
    return []


if __name__ == "__main__":
    # manufacturer_id()

    srl = serial.Serial('COM10')
    srl.baudrate = 115200  # set Baud rate to 9600
    srl.bytesize = 8  # Number of data bits = 8
    srl.parity = 'N'  # No parity
    srl.stopbits = 1  # Number of Stop bits = 1

    time.sleep(1)

    parser = ArgumentParser()
    parser.add_argument('-r', type=int, default=2048, help='Read from the flash sequentially.')
    parser.add_argument('-ra', type=argparse.FileType('r'), help='Read from the flash using a file of addresses.')
    parser.add_argument('-w', type=argparse.FileType('rb'),
                        help='Write to the flash sequentially using data from a file.')
    parser.add_argument('-wa', type=argparse.FileType('r'), help='Write to the flash address-wise using the file.')
    parser.add_argument('-b', type=int, default=115200, help='Baud rate for serial communication. (default: 115200)')
    parser.add_argument('-c', default='COM10', help='Serial port for communication. (default: COM10)')
    args = parser.parse_args()
    args = vars(args)

    if args.get('w'):
        chip_erase(srl)

        print("Write begun.")
        b = write_seq(srl, args['w'])
        if not b:
            print("Write failed.")
            exit(1)

        print(f"{b} bytes written and verified successfully")
    # main()
