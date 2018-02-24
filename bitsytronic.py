import time
import random
import serial
import pygame
import itertools

def grouper(iterable, n, fillvalue=None):
    "Collect data into fixed-length chunks or blocks"
    # grouper('ABCDEFG', 3, 'x') --> ABC DEF Gxx
    args = [iter(iterable)] * n
    return itertools.izip_longest(fillvalue=fillvalue, *args)

def byte_to_bits(data):
    return format(ord(data), '0>8b')

def byte_grid_to_bools(grid):
    return [bit == "1" for bit in "".join(byte_to_bits(byte) for byte in grid)]

MIXER   = [0, 16, 4, 20, 8, 24, 12, 28, 32, 48, 36, 52, 40, 56, 44, 60]
UNMIXER = [0, 8, 16, 24, 4, 12, 20, 28, 32, 40, 48, 56, 36, 44, 52, 60] 

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

    for chunk in grouper(grid, 8, 0):
        value = int("".join(str(int(bit)) for bit in chunk)[::-1], 2)
        pipe.write(chr(value))

def run():
    SCREEN = (480, 272)
    FPS = 20
    EXIT = False

    BLACK = (32, 32, 32)
    WHITE = (255, 255, 255)

    pygame.init()
    pygame.display.set_caption('bitsytronic')
    screen = pygame.display.set_mode(SCREEN)
    clock = pygame.time.Clock()

    frame = 0
    grids = [[False] * 64, [False] * 64]

    pipe = serial.Serial("COM3", timeout=0) 
    data = ""

    while True:
        if "\n" in pipe.read():
            print("done")
            break

        clock.tick(FPS)

    while not EXIT:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                EXIT = True
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_SPACE:
                    frame = 1 - frame
                    send_grid(grids[frame], pipe)

        data += pipe.read(8 - len(data))

        if len(data) == 8:
            grids[frame] = byte_grid_to_bools(data)
            grids[frame] = mix_grid(grids[frame])
            data = ""

        for y in xrange(8):
            for x in xrange(8):
                color = WHITE if grids[frame][y * 8 + x] else BLACK
                pygame.draw.rect(screen, color, (x * 32 + 8, y * 32 + 8, 32, 32))

        pygame.display.flip()

        clock.tick(FPS)

if __name__ == "__main__":
    run()
