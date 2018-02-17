import time
import random
import serial
import pygame

DIGITS = "0123456789ABCDEF"

def random_cell():
    return "".join(random.choice(DIGITS) for _ in xrange(4))

def hex_digit_to_bits(digit):
    return format(int(digit, base=16), '0>4b')

def hex_grid_to_bools(grid):
    return [bit == "1" for bit in "".join(hex_digit_to_bits(digit) for digit in grid)]

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

    grid = [False] * 16
    pipe = serial.Serial("COM3", timeout=0) 
    data = ""

    while not EXIT:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                EXIT = True

        data = pipe.readline().strip()

        if len(data) == 4:
            grid = hex_grid_to_bools(data)
            data = ""

        for y in xrange(4):
            for x in xrange(4):
                color = WHITE if grid[y * 4 + x] else BLACK
                pygame.draw.rect(screen, color, (x * 64 + 8, y * 64 + 8, 64, 64))

        pygame.display.flip()

        clock.tick(FPS)

if __name__ == "__main__":
    run()
