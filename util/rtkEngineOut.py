#!/usr/bin/env python

import sys
import jsonpickle


def main():
    mode = 5
    while 1:
        l = sys.stdin.readline()
        pos = jsonpickle.decode(l)
        if 'pvtOutput' in pos:
            pvt = pos['pvtOutput']
        rover = pos['pvtOutput']['rovPos']['pos']
        if (pvt['ambmode'] == 'Fixed'):
            mode = 1
            print('%s %d %f %f %f %d %d 0 0 0 0 0 0 0 -1' % ('2016/02/04', pvt['time'], rover['x'], rover['y'], rover['z'], mode, pvt['nSats']))

if __name__ == '__main__':
    main()
