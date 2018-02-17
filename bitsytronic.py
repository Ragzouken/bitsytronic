import time
import random
import serial

DIGITS = "0123456789ABCDEF"

def random_cell():
    return "".join(random.choice(DIGITS) for _ in xrange(4))

if __name__ == "__main__":
    pipe = serial.Serial("COM3") 

    while True:
        pipe.write(random_cell())
        time.sleep(0.25)
