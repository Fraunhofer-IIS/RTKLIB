#!/usr/bin/env python

from optparse import OptionParser
import numpy
import sys

parser = OptionParser()
parser.add_option("-i", "--input", help="Position file to evaluate")
parser.add_option("-t", "--threshold", help="Required RTK fix rate in %")
(options, args) = parser.parse_args()

if options.input is None or options.threshold is None:
    parser.print_help()
    sys.exit(1)

sol_type = numpy.genfromtxt(options.input, comments='%', usecols=[5])
rtk_rate = float(numpy.count_nonzero(sol_type == 1)) / len(sol_type) * 100.0

print('RTK fixes: %f percent' % rtk_rate)
if rtk_rate < float(options.threshold):
    sys.exit(1)
