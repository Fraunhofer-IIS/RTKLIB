#!/usr/bin/env python
import matplotlib.pyplot as plt
import numpy as np
import datetime
from math import radians, cos, sin, atan2, sqrt
from optparse import OptionParser
import sys


def haversine(lon1, lat1, lon2, lat2):
    """Calculate the great circle distance between two points
    on the earth (specified in decimal degrees)
    """
    # convert decimal degrees to radians
    lon1, lat1, lon2, lat2 = map(radians, [lon1, lat1, lon2, lat2])

    # haversine formula
    dlon = abs(lon2 - lon1)
    dlat = abs(lat2 - lat1)
    a = sin(dlat/2)**2 + cos(lat1) * cos(lat2) * sin(dlon/2)**2
    c = 2 * atan2(sqrt(a), sqrt(1-a))
    r = 6371000

    return c * r


def difference(ref_lat_list, ref_lon_list, ref_time_list, lat_list,
               lon_list, time_list, Q_list=None):

    if Q_list is not None:
        output_diff_list = []
        output_Q_list = []
        output_time_list = []

        for ii in range(len(ref_time_list)):
            if ref_time_list[ii] in time_list:
                ind = time_list.index(ref_time_list[ii])
                output_diff_list.append(haversine(float(ref_lon_list[ii]),
                                                  float(ref_lat_list[ii]),
                                                  float(lon_list[ind]),
                                                  float(lat_list[ind])))
                output_Q_list.append(Q_list[ind])
            else:
                output_diff_list.append(np.nan)
                output_Q_list.append(np.nan)
            output_time_list.append(ref_time_list[ii])
        return output_diff_list, output_Q_list, output_time_list

    else:
        output_diff_list = []
        output_time_list = []

        for ii in range(len(ref_time_list)):
            if ref_time_list[ii] in time_list:
                ind = time_list.index(ref_time_list[ii])
                output_diff_list.append(haversine(float(ref_lon_list[ii]),
                                                  float(ref_lat_list[ii]),
                                                  float(lon_list[ind]),
                                                  float(lat_list[ind])))
            else:
                output_diff_list.append(np.nan)
            output_time_list.append(ref_time_list[ii])
        return output_diff_list, output_time_list


if __name__ == "__main__":

    #  Option parsing
    usage = "usage: %prog [option] arg1 arg2 "
    parser = OptionParser(usage=usage)
    parser.add_option('-f', '--files',
                      help='Mandatory: Files to evaluate', nargs=2)
    parser.add_option('-l', '--leaps', default=0,
                      help="""If necessary: Enter leap seconds. Not leap
                           seconds needed if both files have same time
                           scale """, nargs=2)
    parser.add_option('-Q', '--quality',
                      help='Optional: 1 or 2 for the selected file Q factor')
    (options, args) = parser.parse_args()

    if not options.files:
        parser.error('File names not given')
    if not options.quality:
        Q_num = 0
    else:
        Q_num = options.quality

    # Data initialization
    files = {}
    files["1"] = {}
    files["1"]["time"] = []
    files["1"]["latitude"] = []
    files["1"]["longitude"] = []
    files["1"]["Q"] = []
    files["2"] = {}
    files["2"]["time"] = []
    files["2"]["latitude"] = []
    files["2"]["longitude"] = []
    files["2"]["Q"] = []

    file1 = np.genfromtxt(options.files[0], dtype='string',
                          delimiter=None, comments='%')
    file2 = np.genfromtxt(options.files[1], dtype='string',
                          delimiter=None, comments='%')

    files["1"]["latitude"] = file1[:, 2]
    files["1"]["longitude"] = file1[:, 3]
    files["1"]["Q"] = file1[:, 5]
    files["2"]["latitude"] = file2[:, 2]
    files["2"]["longitude"] = file2[:, 3]
    files["2"]["Q"] = file2[:, 5]

    # Time conversion according to leap seconds
    if not options.leaps:
        print "No leap seconds given. Both files will have same time scale"
        for ii in range(len(file1[:, 1])):
            files["1"]["time"].append(datetime.datetime.strptime(file1[ii, 1],
                                      '%H:%M:%S.%f').replace(microsecond=0))
        for ii in range(len(file2[:, 1])):
            files["2"]["time"].append(datetime.datetime.strptime(file2[ii, 1],
                                      '%H:%M:%S.%f').replace(microsecond=0))
    else:
        leap1 = options.leaps[0]
        leap2 = options.leaps[1]

        for ii in range(len(file1[:, 1])):
            files["1"]["time"].append(datetime.datetime.strptime(file1[ii, 1],
                                      '%H:%M:%S.%f').replace(microsecond=0) +
                                      datetime.timedelta(0, int(leap1)))
        for ii in range(len(file2[:, 1])):
            files["2"]["time"].append(datetime.datetime.strptime(file2[ii, 1],
                                      '%H:%M:%S.%f').replace(microsecond=0) +
                                      datetime.timedelta(0, int(leap2)))

    # List of coordinate differences, list of timestamps and list of Q factors
    if Q_num != 0:
        diff_list = []
        Q_list = []
        time_list = []
        diff_list, Q_list, time_list = difference(files["1"]["latitude"],
                                                  files["1"]["longitude"],
                                                  files["1"]["time"],
                                                  files["2"]["latitude"],
                                                  files["2"]["longitude"],
                                                  files["2"]["time"],
                                                  files[Q_num]["Q"])
        for ii, Q in enumerate(Q_list):
            try:
                if int(Q) == 1:
                    Q_list[ii] = 'green'
                elif int(Q) == 2:
                    Q_list[ii] = 'yellow'
                elif int(Q) == 5:
                    Q_list[ii] = 'red'
                elif int(Q) == 4:
                    Q_list[ii] = 'blue'
            except:
                Q_list[ii] = 'white'
    else:
        diff_list = []
        time_list = []
        diff_list, time_list = difference(files["1"]["latitude"],
                                          files["1"]["longitude"],
                                          files["1"]["time"],
                                          files["2"]["latitude"],
                                          files["2"]["longitude"],
                                          files["2"]["time"])

    # Mean difference:
    mean = 0
    cont = 0
    for ii in diff_list:
        if ii is not np.nan:
            cont += 1
            mean = mean + ii
    mean = mean / cont
    print "Mean difference: ", mean

    # PLOTTING

    plt.figure()
    plt.title(str(options.files[0]) + " vs " + str(options.files[1]))
    plt.plot(time_list, diff_list)
    if Q_num != 0:
        plt.scatter(time_list, np.zeros(len(diff_list)), c=np.array(Q_list),
                    edgecolor='none')
    plt.xlabel("time")
    plt.ylabel("difference [m]")
    plt.grid()
    plt.show()
