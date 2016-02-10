#!/usr/bin/env python

import sys
import jsonpickle


def main():
    while True:
        l = sys.stdin.readline()
        try:
            pos = jsonpickle.decode(l)
        except ValueError:
            continue
        if 'pvtOutput' not in pos:
            continue
        pvt = pos['pvtOutput']
        if 'roverPos' not in pvt or 'time' not in pvt or 'nSats' not in pvt:
            continue
        if 'pos' not in pvt['roverPos']:
            continue
        rover = pvt['rovPos']['pos']
        mode = 5  # Default single point position
        if 'ambmode' in pvt:
            if (pvt['ambmode'] == 'Fixed'):
                mode = 1
        if 'x' not in rover or 'y' not in rover or 'z' not in rover:
            continue
        print('%s %d %f %f %f %d %d 0 0 0 0 0 0 0 -1' % ('2016/02/04', pvt['time'], rover['x'], rover['y'], rover['z'], mode, pvt['nSats']))

if __name__ == '__main__':
    main()
