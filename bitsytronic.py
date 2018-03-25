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
        self.serial = serial.Serial(port, 115200, timeout=0)
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

def recv_PAD_DOWN(buffer):
    global KEYS, grids

    if not buffer.has_bytes(2):
        return False

    buffer.take_byte()
    button = buffer.take_byte()

    print("pad down: %s" % button)

    KEYS[256 + button] = 1

    print(len(GRAPHICS))
    grids = GRAPHICS[button]
    send_grid([i == button for i in xrange(64)], MESSAGER.serial)

    return True

def recv_PAD_UP(buffer):
    global KEYS

    if not buffer.has_bytes(2):
        return False

    buffer.take_byte()
    button = buffer.take_byte()

    print("pad up: %s" % button)

    KEYS[256 + button] = 0

    return True

def SET_PAD_TOGGLE(toggle):
    global PAD_TOGGLE
    PAD_TOGGLE = toggle

    print("SET PAD TOGGLE TO %s" % PAD_TOGGLE)

    if PAD_TOGGLE:
        MESSAGER.serial.write(chr(6))
        send_grid(grids[frame], MESSAGER.serial)
    else:
        MESSAGER.serial.write(chr(5))
        button = GRAPHICS.index(grids)
        send_grid([i == button for i in xrange(64)], MESSAGER.serial)

def recvBUTTONDOWN(buffer):
    global frame, grids, KEYS, SEL

    if not buffer.has_bytes(2):
        return False

    buffer.take_byte()
    button = buffer.take_byte()

    print("button down: %s" % button)

    KEYS[button] = 1

    if button == 2:
        flip(grids[frame])
        if PAD_TOGGLE:
            send_grid(grids[frame], buffer.serial)
    elif button == 3:
        frame = 1 - frame
        if PAD_TOGGLE:
            send_grid(grids[frame], buffer.serial)
    elif button == 4:
        SEL = (SEL + 1) % 3 
    elif button == 5:
        SET_PAD_TOGGLE(not PAD_TOGGLE)
        save()

    return True

def recvBUTTONUP(buffer):
    global frame, grids, KEYS

    if not buffer.has_bytes(2):
        return False

    buffer.take_byte()
    button = buffer.take_byte()

    KEYS[button] = 0

    if button == 3:
        if PAD_TOGGLE:
            send_grid(grids[frame], buffer.serial)

    print("button up: %s" % button)

    return True

def recvDIALCHANGE(buffer):
    if not buffer.has_bytes(3):
        return False

    buffer.take_byte()
    dial = buffer.take_byte()
    DIALS[dial] = buffer.take_byte()

    #print(DIALS[dial])

COMMANDS = {
    0: recvDEBUG,
    1: recvSYNCGRID,
    2: recvBUTTONDOWN,
    3: recvBUTTONUP,
    4: recvDIALCHANGE,
    # pad toggle
    7: recv_PAD_DOWN,
    8: recv_PAD_UP,
}

def flip(grid):
    copy = grid[:]

    for y in xrange(8):
        for x in xrange(8):
            grid[y * 8 + x] = copy[y * 8 + 7 - x]

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
HUE = 0
SEL = 0
HSV = [0, 255, 255]
PAD_TOGGLE = True


def save():
    with open('graphics.txt', 'w') as outfile:
        json.dump(GRAPHICS, outfile)

def load():
    with open('graphics.txt', 'r') as infile:
        return json.load(infile)

def run():
    global frame, grids, HUE, GRAPHICS
    SCREEN = (800, 600)
    #SCREEN = (480, 272)
    FPS = 20 # 50ms per frame
    FRAME = 0
    EXIT = False

    BLACK = (32, 32, 32)
    WHITE = (255, 255, 255)

    pygame.init()
    pygame.mouse.set_visible(False)
    pygame.display.set_caption('bitsytronic')
    screen = pygame.display.set_mode(SCREEN, pygame.FULLSCREEN)
    clock = pygame.time.Clock()

    try:
        GRAPHICS = load()
        grids = GRAPHICS[0]

        while len(GRAPHICS) < 64:
            GRAPHICS.append([[False] * 64, [False] * 64])

        print(len(GRAPHICS))
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

        if 2 in DIALS and DIALS[2] != 128:
            change = DIALS[2] - 128
            
            if SEL == 0:
                HSV[SEL] = (HSV[SEL] + change * 4) % 256
            else:
                HSV[SEL] = min(max(0, HSV[SEL] + change * 4), 255)

            print(HSV)
            DIALS[2] = 128

        r, g, b = colorsys.hsv_to_rgb(HSV[0] / 255., HSV[1] / 255., HSV[2] / 255.)
        fore = (r * 255, g * 255, b * 255)
        dim = (r * 128, g * 128, b * 128)

        animate = (not PAD_TOGGLE) or (3 in KEYS and KEYS[3] > 8)

        if animate:
            frame = (FRAME // 8) % 2

            if PAD_TOGGLE:
                send_grid(grids[frame], MESSAGER.serial)
            
        if animate or (3 in KEYS and KEYS[3] > 0):
            dim = BLACK

        screen.fill((96, 96, 96))

        for y in xrange(8):
            for x in xrange(8):
                color = fore if grids[frame][y * 8 + x] else (dim if grids[1 - frame][y * 8 + x] else BLACK)
                pygame.draw.rect(screen, color, (x * 64 + 144, y * 64 + 44, 64, 64))

        pygame.display.flip()

        clock.tick(FPS)
        FRAME += 1

        for key in KEYS:
            if KEYS[key] > 0:
                KEYS[key] += 1

if __name__ == "__main__":
    run()
