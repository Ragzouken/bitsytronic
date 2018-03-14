import time
import random
import serial
import pygame
import itertools

import argparse
import json
import colorsys

class SerialMessager(object):
    def __init__(self):
        self.serial = None
        self.data = []

    def connect(self, port):
        self.serial = serial.Serial(port, timeout=0)
        self.data = []

    def receive(self):
        self.data.extend(ord(c) for c in self.serial.read(255))

    def process(self):
        if self.data:
            return COMMANDS[self.data[0]](self)

    def peek(self, count):
        return self.data[:count]

    def has_bytes(self, count):
        return len(self.data) >= count

    def take_byte(self):
        return self.take_bytes(1)[0]

    def take_string(self, count):
        return "".join(chr(c) for c in self.take_bytes(count))

    def take_bytes(self, count):
        data = self.data[:count]
        del self.data[:count]
        return data

class Project(object):
    def __init__(self):
        self.graphics = []

def recvDEBUG(buffer):
    if buffer.has_bytes(2) and buffer.has_bytes(buffer.data[1] + 2):
        buffer.take_byte()
        count = buffer.take_byte()
        print(buffer.take_string(count))
        return True

    return False

def recvSYNCGRID(buffer):
    global frame, grids

    if not buffer.has_bytes(9):
        return False

    buffer.take_byte()

    grids[frame] = byte_grid_to_bools(buffer.take_bytes(8))
    grids[frame] = mix_grid(grids[frame])

    return True

def recvBUTTONDOWN(buffer):
    global frame, grids, KEYS

    if not buffer.has_bytes(2):
        return False

    buffer.take_byte()
    button = buffer.take_byte()

    print("button down: %s" % button)

    KEYS[button] = 1

    if button == 3:
        frame = 1 - frame
        send_grid(grids[frame], buffer.serial)

    return True

def recvBUTTONUP(buffer):
    global frame, grids, KEYS

    if not buffer.has_bytes(2):
        return False

    buffer.take_byte()
    button = buffer.take_byte()

    KEYS[button] = 0

    if button == 3:
        send_grid(grids[frame], buffer.serial)

    print("button up: %s" % button)

    return True

def recvDIALCHANGE(buffer):
    if not buffer.has_bytes(3):
        return False

    buffer.take_byte()
    dial = buffer.take_byte()
    DIALS[dial] = buffer.take_byte()

COMMANDS = {
    0: recvDEBUG,
    1: recvSYNCGRID,
    2: recvBUTTONDOWN,
    3: recvBUTTONUP,
    4: recvDIALCHANGE,
}

def grouper(iterable, n, fillvalue=None):
    "Collect data into fixed-length chunks or blocks"
    # grouper('ABCDEFG', 3, 'x') --> ABC DEF Gxx
    args = [iter(iterable)] * n
    return itertools.izip_longest(fillvalue=fillvalue, *args)

def byte_to_bits(data):
    return format(data, '0>8b')

def byte_grid_to_bools(grid):
    return [bit == "1" for bit in "".join(byte_to_bits(byte) for byte in grid)]

MIXER   = [0, 16, 4, 20, 8, 24, 12, 28, 32, 48, 36, 52, 40, 56, 44, 60]
UNMIXER = [0, 8, 16, 24, 4, 12, 20, 28, 32, 40, 48, 56, 36, 44, 52, 60] 
GRAPHICS = []
KEYS = {}
DIALS = {0: 255}

def mix_grid(grid):
    next = [False] * 64

    for r, row in enumerate(MIXER):
        for i in xrange(4):
            next[r * 4 + i] = grid[row + i]

    return next

def unmix_grid(grid):
    next = [False] * 64

    for r, row in enumerate(UNMIXER): 
        for i in xrange(4):
            next[r * 4 + i] = grid[row + i]

    return next

def send_grid(grid, pipe):
    grid = unmix_grid(grid)

    pipe.write(chr(1))

    for chunk in grouper(grid, 8, 0):
        value = int("".join(str(int(bit)) for bit in chunk)[::-1], 2)
        pipe.write(chr(value))


frame = 0
grids = [[False] * 64, [False] * 64]

MESSAGER = SerialMessager()

def run():
    global frame, grids
    #SCREEN = (800, 600)
    SCREEN = (480, 272)
    FPS = 20 # 50ms per frame
    FRAME = 0
    EXIT = False

    BLACK = (32, 32, 32)
    WHITE = (255, 255, 255)

    pygame.init()
    pygame.display.set_caption('bitsytronic')
    screen = pygame.display.set_mode(SCREEN)
    clock = pygame.time.Clock()

    def save():
        with open('graphics.txt', 'w') as outfile:
            json.dump([grids], outfile)

    def load():
        with open('graphics.txt', 'r') as infile:
            return json.load(infile)

    try:
        grids = load()[0]
    except (IOError, ValueError):
        pass

    parser = argparse.ArgumentParser(description='bitsytronic')
    parser.add_argument('--port', '-p', dest='serial_port', default="COM3")
    args = parser.parse_args()

    MESSAGER.connect(args.serial_port)

    while True:
        MESSAGER.receive()
        if MESSAGER.process():
            break

        clock.tick(FPS)

    send_grid(grids[frame], MESSAGER.serial)

    while not EXIT:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                EXIT = True
                save()
                send_grid([False] * 64, MESSAGER.serial)
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    EXIT = True
                    save()
                    send_grid([False] * 64, MESSAGER.serial)

        MESSAGER.receive()
        MESSAGER.process()

        if 3 in KEYS and KEYS[3] >= 8:
            frame = (FRAME // 8) % 2
            send_grid(grids[frame], MESSAGER.serial)

        for y in xrange(8):
            for x in xrange(8):
                r, g, b = colorsys.hsv_to_rgb(DIALS[0] / 255., .75, 1)
                fore = (r * 255, g * 255, b * 255)
                color = fore if grids[frame][y * 8 + x] else BLACK
                pygame.draw.rect(screen, color, (x * 32 + 8, y * 32 + 8, 32, 32))

        pygame.display.flip()

        clock.tick(FPS)
        FRAME += 1

        for key in KEYS:
            if KEYS[key] > 0:
                KEYS[key] += 1

if __name__ == "__main__":
    run()
