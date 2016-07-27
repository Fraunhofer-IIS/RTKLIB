import numpy as np
import sys
import datetime
import re


def weeksecondstoutc(gpsweek, gpsseconds):

    """This function changes the data from Number-of-Week and
    Milliseconds-into-week format to the YYYY/MM/DD HH:MM:SS format
    referenciated to 1980/01/06 00:00:00.000000
    """

    datetimeformat = "%Y/%m/%d %H:%M:%S.%f"
    epoch = datetime.datetime.strptime("1980/01/06 00:00:00.000000",
    datetimeformat)
    elapsed = datetime.timedelta(days=(gpsweek * 7), seconds=(gpsseconds))
    return datetime.datetime.strftime(epoch + elapsed, datetimeformat)[: -3]

if __name__ == "__main__":

    """The function expects a .prn file (output format of ADMA receiver)
    as an iput argument and has as an output a .pos file
    (readable by rtkplot)
    """

    #creation of the output files

    data_file = raw_input("name of the .prn file to convert: ")
    data_file_input_extension = ".prn"
    data_file_output_extension = ".pos"

    output_file_gps = open(data_file + "_gps" +
    data_file_output_extension, 'w')

    output_file_inertia = open(data_file + "_inertia" +
    data_file_output_extension, 'w')

    with open(data_file + "_PrnList.txt") as file:

        #data to be used

        data = np.genfromtxt(data_file + data_file_input_extension,
        delimiter=',')
        info = []
        parameters = []
        Q_factor_gps = np.zeros(len(data[:, 0]))
        Q_factor_inertia = np.zeros(len(data[:, 0]))

        info.append("GPS_Pos_Latitude")
        info.append("GPS_Pos_Longitude")
        info.append("GPS_Pos_Stddev_Latitude")
        info.append("GPS_Pos_Stddev_Longitude")
        info.append("GPS_Pos_Stddev_Height")
        info.append("GPS_Aux_Satellites_Visible")
        info.append("GPS_Pos_Height")
        info.append("INS_Pos_Height")
        info.append("ADMA_Time_Milliseconds")
        info.append("ADMA_Time_Week")
        info.append("INS_Pos_Latitude")
        info.append("INS_Pos_Longitude")
        info.append("INS_Pos_Stddev_Latitude")
        info.append("INS_Pos_Stddev_Longitude")
        info.append("INS_Pos_Stddev_Height")

    #checking if all of the needed messages are in the txt file
    #any time a message is found, the values are copied
    #if it is not found, null values are used

        for ii in range(0, 15):
            flag = 0

            for line in file:
                if re.search(info[ii], line):

                    equality = line.rfind('=')
                    flag = 1

                    parameters.append(data[:, int(line[equality + 1:
                    equality + 3])].T)

                    break

            if flag == 0:

                parameters.append(np.zeros(len(data[:, 0])))

        gps_estimated_error = (parameters[2][:] ** 2 + parameters[3][:]
        ** 2 + parameters[4][:] ** 2) ** 0.5

        inertia_estimated_error = (parameters[12][:] ** 2 + parameters[13][:]
        ** 2 + parameters[14][:] ** 2) ** 0.5

    #processing of the Q:

        if (sum(gps_estimated_error) == 0):

            Q_factor_gps[:] = 6

        else:

            for error, kk in zip(gps_estimated_error,
            range(0, len(Q_factor_gps))):

                if (0 < error and error < 0.15):

                    Q_factor_gps[kk] = 1

                elif (0.15 < error and error < 2):

                    Q_factor_gps[kk] = 2

                elif (2 < error and error < 8):

                    Q_factor_gps[kk] = 4

                else:

                    Q_factor_gps[kk] = 5

        if (sum(inertia_estimated_error) == 0):

            Q_factor_inertia[:] = 6

        else:

            for error, kk in zip(inertia_estimated_error,
            range(0, len(Q_factor_inertia))):

                if (0 < error and error < 0.15):

                    Q_factor_inertia[kk] = 1

                elif (0.15 < error and error < 2):

                    Q_factor_inertia[kk] = 2

                elif (2 < error and error < 8):

                    Q_factor_inertia[kk] = 4

                else:

                    Q_factor_inertia[kk] = 5

    #writing in the output files

    output_file_gps.write("% (lat/lon/height=WGS84/ellipsoidal,Q=1:fix," +
    "2:float,3:sbas,4:dgps,5:single,6:ppp,ns=# of satellites)   \n")

    output_file_gps.write("%  GPST                      latitude(deg) " +
    "longitude(deg)  height(m)   Q  ns   sdn(m)   sde(m)   sdu(m)" +
    "  sdne(m)  sdeu(m)  sdun(m) age(s)  ratio     \n")

    output_file_inertia.write("% (lat/lon/height=WGS84/ellipsoidal,Q=1:fix" +
    ",2:float,3:sbas,4:dgps,5:single,6:ppp,ns=# of satellites)   \n")

    output_file_inertia.write("%  GPST                      latitude(deg) " +
    "longitude(deg)  height(m)   Q  ns   sdn(m)   sde(m)   sdu(m)" +
    " sdne(m)  sdeu(m)  sdun(m) age(s)  ratio     \n")

    values = range(0, len(data[:, 0]))

    for week, sec, ii in zip(parameters[9], parameters[8] / 1000, values):

        if week > 1000:

            output_file_gps.write(weeksecondstoutc(week, sec))
            output_file_gps.write("      ")
            output_file_gps.write('%s' % '{:12.9f}'.
            format(parameters[0][ii]))
            output_file_gps.write("   ")
            output_file_gps.write('%s' % '{:12.9f}'.
            format(parameters[1][ii]))
            output_file_gps.write("   ")
            output_file_gps.write('%s' % '{:8.4f}'.
            format(parameters[6][ii]))
            output_file_gps.write("    ")
            output_file_gps.write('%s' %
            int(Q_factor_gps[ii]))
            output_file_gps.write("  ")
            output_file_gps.write('%s' % "%02d" %
            int(parameters[5][ii]))
            output_file_gps.write("  ")
            output_file_gps.write('%s' % '{:7.4f}'.
            format(parameters[2][ii]))
            output_file_gps.write("  ")
            output_file_gps.write('%s' % '{:7.4f}'.
            format(parameters[3][ii]))
            output_file_gps.write("  ")
            output_file_gps.write('%s' % '{:7.4f}'.
            format(parameters[4][ii]))
            output_file_gps.write("   ")

            output_file_gps.write("\n")

            output_file_inertia.write(weeksecondstoutc(week, sec))
            output_file_inertia.write("      ")
            output_file_inertia.write('%s' % '{:12.9f}'.
            format(parameters[10][ii]))
            output_file_inertia.write("   ")
            output_file_inertia.write('%s' % '{:12.9f}'.
            format(parameters[11][ii]))
            output_file_inertia.write("   ")
            output_file_inertia.write('%s' % '{:8.4f}'.
            format(parameters[7][ii]))
            output_file_inertia.write("    ")
            output_file_inertia.write('%s' %
            int(Q_factor_inertia[ii]))
            output_file_inertia.write("  ")
            output_file_inertia.write('%s' % "%02d" %
            int(parameters[5][ii]))
            output_file_inertia.write("  ")
            output_file_inertia.write('%s' % '{:7.4f}'.
            format(parameters[12][ii]))
            output_file_inertia.write("  ")
            output_file_inertia.write('%s' % '{:7.4f}'.
            format(parameters[13][ii]))
            output_file_inertia.write("  ")
            output_file_inertia.write('%s' % '{:7.4f}'.
            format(parameters[14][ii]))
            output_file_inertia.write("   ")

            output_file_inertia.write("\n")

    output_file_gps.close()
    output_file_inertia.close()
