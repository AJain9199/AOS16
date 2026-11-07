import random

MAX_BYTES = 0x10000
DATA_SZ = 0xFFFF

def mangler(data):
    return (~data) & (DATA_SZ)

with open('test.hex', 'w') as f:
    f.write("v3.0 hex words addressed\n")
    for i in range(0, MAX_BYTES, 8):
        f.write(f"{i:04x}:")
        for extra in range(8):
            addr = i + extra 
            word = mangler(addr) 
            f.write(f" {word:04x}")
        f.write("\n")
