#!/usr/bin/python

import os, sys

FRAMES_FOLDER = 'frames'
MIN_BITRATE = 32
MAX_BITRATE = 240
INTEGER_BITRATE = True
EXPONENT_BASE = 6
PRINT_BITRATE_ONLY = len(sys.argv) > 1

files = sorted(os.listdir(FRAMES_FOLDER))
N = len(files) - 1
#~ N = 1309

def exp_func(i):
    bitrate = MIN_BITRATE + (MAX_BITRATE-MIN_BITRATE) \
              * (EXPONENT_BASE**(i/N) - 1) / (EXPONENT_BASE-1)
    return bitrate


for i,f in enumerate(files):
    b = exp_func(i)
    if INTEGER_BITRATE:
        b = round(b)
    if PRINT_BITRATE_ONLY:
        print(b)
    else:
        print(f'{FRAMES_FOLDER}/{f}', b)
    
