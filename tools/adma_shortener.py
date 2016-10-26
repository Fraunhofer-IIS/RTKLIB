#!/usr/bin/env python
import numpy as np
from datetime import datetime
import sys


if __name__ == "__main__":

    adma_file = np.genfromtxt(sys.argv[1], dtype='string', delimiter=None,
                              comments='%')

    adma_date = []
    adma_time = []
    adma_latitudes = []
    adma_longitudes = []
    adma_heights = []
    adma_Q = []
    adma_num_satellites = []
    adma_sdn = []
    adma_sde = []
    adma_sdu = []

    adma_date.extend(adma_file[:, 0])
    adma_time.extend(adma_file[:, 1])
    adma_latitudes.extend(adma_file[:, 2])
    adma_longitudes.extend(adma_file[:, 3])
    adma_heights.extend(adma_file[:, 4])
    adma_Q.extend(adma_file[:, 5])
    adma_num_satellites.extend(adma_file[:, 6])
    adma_sdn.extend(adma_file[:, 7])
    adma_sde.extend(adma_file[:, 8])
    adma_sdu.extend(adma_file[:, 9])

    for ii in range(len(adma_time)):
        adma_time[ii] = datetime.strptime(
            str(adma_date[ii]) + " " + str(adma_time[ii]),
            '%Y/%m/%d %H:%M:%S.%f').replace(microsecond=0)

    ref_time = adma_time[0]
    new_time = []
    new_lat = []
    new_lon = []
    new_h = []
    new_Q = []
    new_num_sat = []
    new_sdn = []
    new_sde = []
    new_sdu = []

    for ii in range(len(adma_time)):
        if ref_time.second != adma_time[ii].second:
            new_time.append(adma_time[ii].strftime(
                            "%Y/%m/%d %H:%M:%S.%f")[:-3])
            new_lat.append(float(adma_latitudes[ii]))
            new_lon.append(float(adma_longitudes[ii]))
            new_h.append(float(adma_heights[ii]))
            new_Q.append(adma_Q[ii])
            new_num_sat.append(adma_num_satellites[ii])
            new_sdn.append(adma_sdn[ii])
            new_sde.append(adma_sde[ii])
            new_sdu.append(adma_sdu[ii])
            ref_time = adma_time[ii]

    new_file = open(sys.argv[1][:-4] + "_new.pos", "w")
    line1 = ("%", "(lat/lon/height=WGS84/ellipsoidal,",
             "Q=1:fix,2:float,3:sbas,4:dgps,5:single,6:ppp",
             ",ns=# of satellites)\n")
    new_file.write("%1s%35s%44s%20s" % line1)
    line2 = ("%", "GPST", "latitude(deg)", "longitude(deg)", "height(m)",
             "Q", "ns", "sdn(m)", "sde(m)", "sdu(m)", "sdne(m)", "sdeu(m)",
             "sdun(m)", "age(s)", "ratio\n")
    new_file.write("%1s%6s%35s%15s%11s%4s%4s%9s%9s%9s%8s%9s%9s%7s%8s" % line2)

    for ii in range(len(new_time)):
        new_file.write("%23s%18s%15s%11s%5s%4s%9s%9s%9s\n" % (new_time[ii],
                       "%.9f" % new_lat[ii], "%.9f" % new_lon[ii],
                       "%.4f" % new_h[ii], int(new_Q[ii]),
                       "%02d" % int(new_num_sat[ii]),
                       "%.4f" % float(new_sdn[ii]),
                       "%.4f" % float(new_sde[ii]),
                       "%.4f" % float(new_sdu[ii])))

    new_file.close()
