import time
import random
import serial
import pygame
import itertools

import argparse
import json

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
            return COMMANDS[self.data[0]](self.data)

def splitTake(buffer, count):
    output = buffer[:count]
    del buffer[:count]
    return output

def recvDEBUG(buffer):
    if len(buffer) > 1 and len(buffer) - 2 >= buffer[1]:
        count = buffer[1]
        del buffer[:2]
        print("".join(chr(c) for c in buffer[:count]))
        del buffer[:count]
        return True

    return False

def recvSYNCGRID(buffer):
    global frame, grids

    if len(buffer) < 9:
        return False

    grids[frame] = byte_grid_to_bools(buffer[1:])
    grids[frame] = mix_grid(grids[frame])

    del buffer[:9]

    return True

def recvBUTTONDOWN(buffer):
    global frame, grids, MESSAGER

    if len(buffer) < 2:
        return False

    print("button down: %s" % buffer[1])

    if buffer[1] == 3:
        frame = 1 - frame
        send_grid(grids[frame], MESSAGER.serial)

    del buffer[:2]

    return True

def recvBUTTONUP(buffer):
    if len(buffer) < 2:
        return False

    print("button up: %s" % buffer[1])

    del buffer[:2]

    return True

COMMANDS = {
    0: recvDEBUG,
    1: recvSYNCGRID,
    2: recvBUTTONDOWN,
    3: recvBUTTONUP,
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
    FPS = 20
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
                if event.key == pygame.K_SPACE:
                    frame = 1 - frame
                    send_grid(grids[frame], MESSAGER.serial)

        MESSAGER.receive()
        MESSAGER.process()

        for y in xrange(8):
            for x in xrange(8):
                color = WHITE if grids[frame][y * 8 + x] else BLACK
                pygame.draw.rect(screen, color, (x * 32 + 8, y * 32 + 8, 32, 32))

        pygame.display.flip()

        clock.tick(FPS)

if __name__ == "__main__":
    run()
