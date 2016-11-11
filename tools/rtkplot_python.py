import numpy as np
import gmplot
import os
from collections import Counter
import subprocess
import re
from tabulate import tabulate


def printLaps(latitudes, longitudes, average_startlat, average_startlong,
              colors, printed_laps, html_data_flag):

    """This function prints the laps that have been recorded"""

    """input: list of latitudes, list of longitudes,
       list of colors, list of the laps to print. The four
       lists have the same length, so that every position coordinate
       has its own color and lap number"""

    gmap = gmplot.GoogleMapPlotter(average_startlat,
                                   average_startlong, 14)
    for files in range(len(latitudes)):
        for lap in printed_laps[files]:
            for ii in range(len(latitudes[files])):

                if lap == laps[files][ii]:
                    lats = [latitudes[files][ii], latitudes[files][ii]
                            + 0.00000001]
                    longs = [longitudes[files][ii], longitudes[files][ii]
                             + 0.000000001]
                    gmap.plot(lats, longs, color=colors[files][ii],
                              edge_width=5)

    gmap.draw(output_filename)

    if html_data_flag == 0:
        write_html_tools()
        html_data_flag = 1

    return html_data_flag


def options(printed_laps, laps):

    """This function gives the chance to the user to plot more
       laps, delete any of the plotted ones and to quit from
       running the script"""

    """input: list of laps to print and list of laps in each file"""

    # option selection and splitting of the data
    input = raw_input("\n-command -filename -lap: ")
    input = input.strip()
    indexes = [m.start() for m in re.finditer('-', input)]  # find all the '-'

    if len(indexes) >= 3:
        while(len(indexes) > 3):
            for ii in indexes:
                # we want to remove all the '-' except from the principal ones
                if (ii != 0) or input[ii - 1] == "-":
                    indexes.remove(ii)
        command = input[indexes[0] + 1: indexes[1]].strip()
        file = input[indexes[1] + 1: indexes[2]].strip()
        lap = input[indexes[2] + 1:].strip()

    elif len(indexes) == 2:
        command = input[indexes[0] + 1: indexes[1]].strip()
        file = input[indexes[1] + 1:].strip()
        lap = 0

    elif len(indexes) == 1:
        command = input[indexes[0] + 1:].strip()
        file = 0
        lap = 0

    if (file == "all"):
        file = -1

    if (lap == "all"):
        lap = -1

    lap = int(lap)
    lap -= 1  # because the user doesn't take into account the 0 lap

    if file != -1 and file != 0:
        # looks for the file name in the list of filenames
        file_index_list = [i for i, x in enumerate(filenames) if x == file]
        file_index_int = file_index_list[0]

    elif file == -1:
        file_index_int = -1

    """option: help"""

    # if any word with an 'h' is entered, we understand that the user tried
    # to enter a 'help' command

    if "h" in command:
        print "\nHELP:"
        print "\n-Command  -> -plot -delete -rtk -quit"
        print "-filename -> name of the file with .pos extension"
        print "-lap      -> integer number that represents the lap number"
        print "\nEnter 'all' in both -filename or -lap to get everything"
        print "\nExample: -plot -file.pos -1\n"

    """option: plot"""

    if (command == "plot"):

        if file_index_int != -1 and file_index_int != 0:
            # laps of specific file
            if lap == -2:  # all laps in the file will be plotted

                for ii in range(max(laps[file_index_int]) + 1):
                    printed_laps[file_index_int].append(ii)

            elif lap != -2 and lap != 0:  # specified laps will be plotted
                if lap not in printed_laps[file_index_int]:

                    printed_laps[file_index_int].append(lap)

        elif file_index_int == -1:  # all files to plot
            for files in range(len(filenames)):
                for l in range(max(laps[files]) + 1):
                    printed_laps[files].append(l)

        return printed_laps, 0

    """option: delete"""

    if (command == "delete"):

        if file_index_int != -1 and file_index_int != 0:
            # plot laps of a specific file
            if lap == -2:  # all laps in the file will be deleted

                for ii in range(len(printed_laps[file_index_int])):
                    printed_laps[file_index_int].pop()

            elif lap != -2 and lap != -1:  # specified laps will be deleted
                if lap in printed_laps[file_index_int]:

                    printed_laps[file_index_int].remove(lap)

        elif file_index_int == -1:  # delete all files
            printed_laps = []
            for files in range(len(filenames)):
                printed_laps.append([])

        return printed_laps, 0

    """option: quit"""

    if command == "quit":
        return printed_laps, -1

    """option: view rtk quality"""

    if (command == "rtk"):
        lap_list = []
        for ii in range(len(laps)):
            lap_list.append(laps[ii][-1])

        rtk_data_list = zip(filenames, quality_percentage, lap_list)
        rtk_data_list = sorted(rtk_data_list, key=lambda rtk_data_list:
                               (rtk_data_list[1][0], rtk_data_list[1][1],
                                rtk_data_list[1][2]), reverse=True)

        title = ["file name", "Q factors [fix, float, single]",
                 " number of laps recorded"]
        print "\n"
        print tabulate(rtk_data_list, headers=title)
        print "\n"
        return printed_laps, -2


def write_html_tools():

    """This function writes the measuring tools to the html file.
       There is no input argument needed"""

    # read file to change html properties:
    file_r = open(output_filename, "r")
    read_file_begin = file_r.readlines()
    read_file_begin1 = read_file_begin[:7]
    read_file_begin2 = read_file_begin[7:]
    file_r.close()

    file_w = open(output_filename, "w")
    file_w.writelines([item for item in read_file_begin1])
    file_w.write("        var map;\n")
    file_w.writelines([item for item in read_file_begin2])
    file_w.close()

    file_r = open(output_filename, "r")
    read_file_begin = file_r.readlines()
    read_file_begin1 = read_file_begin[:13]
    read_file_begin2 = read_file_begin[13:]
    file_r.close()

    file_w = open(output_filename, "w")
    file_w.writelines([item for item in read_file_begin1])
    file_w.write("			scaleControl:true,\n")
    file_w.writelines([item for item in read_file_begin2])
    file_w.close()

    file_r = open(output_filename, "r")
    read_file_begin = file_r.readlines()
    read_file_begin1 = read_file_begin[:16]
    read_file_begin2 = read_file_begin[17:]
    file_r.close()

    file_w = open(output_filename, "w")
    file_w.writelines([item for item in read_file_begin1])
    file_w.write('		map = new google.maps.Map('
                 'document.getElementById("map_canvas"), myOptions);\n')

    file_w.writelines([item for item in read_file_begin2])
    file_w.close()
    # read file and erase end
    file_r = open(output_filename, "r")
    read_file = file_r.readlines()
    read_file_end = read_file[:-8]
    file_r.close()

    # read file to write
    file_w = open(output_filename, "r+")
    file_w.writelines([item for item in read_file_end])
    needed_html_data = """map.addListener('zoom_changed', function()
                       {
                           resize();
                       });

                       map.addListener('center_changed', function()
                       {
                           resize();
                       });

                       }

                       var initialViewportCoordinates = {
                           north: 51.0,
                           east:  5.0,
                           south: 50.0,
                           west: 3.0
                       };

                       function setvalues()
                       {
                       axis_offset = (map.getBounds().getNorthEast().lat() -
                                      map.getBounds().getSouthWest().lat()) /
                                      10;
                       initialViewportCoordinates.north =
                       map.getBounds().getNorthEast().lat() - axis_offset;
                       initialViewportCoordinates.east =
                       map.getBounds().getNorthEast().lng() - axis_offset;
                       initialViewportCoordinates.south =
                       map.getBounds().getSouthWest().lat() + axis_offset;
                       initialViewportCoordinates.west =
                       map.getBounds().getSouthWest().lng() + axis_offset;

                       }
                       var axis_drawed = 0;
                       var lineObjects = [];
                       var line;

                       function drawPolyline(path, color)
                       {
                           line = new google.maps.Polyline(
                           {
                           path: path,
                           draggable: true,
                           strokeColor: color,
                           strokeOpacity: 0.9,
                           strokeWeight: 3
                           });
                           line.setMap(map);

                           // drag event
                           google.maps.event.addDomListener(line, 'drag',
                           function(e){
                           // find out which line is being dragged
                           var index = lineObjects.indexOf(this);

                           // update initialViewportCoordinates
                           switch(index) {
                           case 0: initialViewportCoordinates.north =
                               e.latLng.lat(); break;
                           case 1: initialViewportCoordinates.east =
                               e.latLng.lng(); break;
                           case 2: initialViewportCoordinates.south =
                               e.latLng.lat(); break;
                           case 3: initialViewportCoordinates.west =
                               e.latLng.lng(); break;
                           }
                           displayDifference();
                       });
                       return line;
                       }

                   function displayDifference()
                   {

                   //haversine formula to convert distances:
                   //data:
                   var R = 6371000; // metres
                   var lat_1 = initialViewportCoordinates.south * Math.PI /
                               180;
                   var lat_2 = initialViewportCoordinates.north * Math.PI /
                               180;
                   var delta_lat_lats = lat_2 - lat_1;
                   var delta_lat_lngs = 0;
                   var delta_lng_lats = 0;
                   var delta_lng_lngs = (initialViewportCoordinates.east -
                                         initialViewportCoordinates.west) *
                                         Math.PI / 180;

                   //latitude:
                   var a_lat = Math.sin(delta_lat_lats/2) *
                   Math.sin(delta_lat_lats/2) + Math.cos(lat_1) *
                   Math.cos(lat_2) * Math.sin(delta_lng_lats/2) *
                   Math.sin(delta_lng_lats/2);
                   var c_lat = 2 * Math.atan2(Math.sqrt(a_lat),
                                   Math.sqrt(1-a_lat));
                   var d_lat = R * c_lat;

                   //longitude:
                   var a_lng = Math.sin(delta_lat_lngs/2) *
                   Math.sin(delta_lat_lngs/2) + Math.cos(lat_1) *
                   Math.cos(lat_2) * Math.sin(delta_lng_lngs/2) *
                   Math.sin(delta_lng_lngs/2);
                   var c_lng = 2 * Math.atan2(Math.sqrt(a_lng),
                                   Math.sqrt(1-a_lng));
                   var d_lng = R * c_lng;

                   document.getElementById('log').innerHTML =

                   'difference lat: ' + Math.abs(d_lat) + ' [m] <br/>' +
                   'difference lng: ' + Math.abs(d_lng) +
                   ' [m]'
                   ;
                   }

                   function drawViewport()
                   {
                   var extraDegrees = (map.getBounds().getNorthEast().lat() -
                                       map.getBounds().getSouthWest().lat());

                   var north_line = [
                   {lat: initialViewportCoordinates.north ,
                    lng: initialViewportCoordinates.east + extraDegrees},
                   {lat: initialViewportCoordinates.north,
                    lng: initialViewportCoordinates.west - extraDegrees}
                   ];
                   var east_line = [
                   {lat: initialViewportCoordinates.north + extraDegrees,
                    lng: initialViewportCoordinates.east},
                   {lat: initialViewportCoordinates.south - extraDegrees,
                    lng: initialViewportCoordinates.east}
                   ];
                   var south_line = [
                   {lat: initialViewportCoordinates.south ,
                    lng: initialViewportCoordinates.east + extraDegrees},
                   {lat: initialViewportCoordinates.south,
                    lng: initialViewportCoordinates.west - extraDegrees}
                   ];
                    var west_line = [
                   {lat: initialViewportCoordinates.north + extraDegrees,
                    lng: initialViewportCoordinates.west},
                   {lat: initialViewportCoordinates.south - extraDegrees,
                    lng: initialViewportCoordinates.west}
                   ];

                   // we will genetate the lines and store the resulting
                   // objects in this array

                   lineObjects = [
                   drawPolyline(north_line, '#ff0000'),
                   drawPolyline(east_line, '#ff0000'),
                   drawPolyline(south_line, '#ff0000'),
                   drawPolyline(west_line, '#ff0000')
                   ];
                   }

                   function draw_axis()
                   {
                       if (axis_drawed == 0)
                       {
                           axis_drawed = 1;
                           setvalues();
                           drawViewport();
                           displayDifference();
                       }
                       else
                       {
                           document.getElementById('log').innerHTML = '';
                           lineObjects[0].setMap(null);
                           lineObjects[1].setMap(null);
                           lineObjects[2].setMap(null);
                           lineObjects[3].setMap(null);
                           axis_drawed = 0;
                       }
                   }

                   function resize()
                   {
                       if(axis_drawed == 1)
                       {
                           document.getElementById('log').innerHTML = '';
                           lineObjects[0].setMap(null);
                           lineObjects[1].setMap(null);
                           lineObjects[2].setMap(null);
                           lineObjects[3].setMap(null);

                           setvalues();
                           drawViewport();
                           displayDifference();
                       }
                   }

                   google.maps.event.addDomListener(window, 'load',initialize);

                   </script>
                   </head>
                   <body>

                   <div style="text-align:center;">
                   <button onclick="draw_axis()">Enable/Disable Axis</button>
                   <br><br>  <div id="log"></div>
                   </div>

                   <div id="map_canvas" style="width: 100%; height: 90%;">
                   </div>

                   </body>
                   </html>"""

    file_w.write(needed_html_data)
    file_w.close()

if __name__ == "__main__":

    """This script prints the route recorded in all the
       .pos files located in a directory. The output
       is given in a .html file that can be opened to see the plotted data
       and has a measuring tool"""

    datas = []
    latitudes = []
    longitudes = []
    printed_laps = []
    filenames = []
    Q_factors = []
    quality_percentage = []

    # read data from file

    path = raw_input("\npath: ")

    for filename in os.listdir(path):
        if filename.endswith('.pos'):

            f = os.path.join(path, filename)

            datas.append([])
            latitudes.append([])
            longitudes.append([])
            printed_laps.append([])
            filenames.append(filename)
            Q_factors.append([])
            quality_percentage.append([])

            with open(f):
                try:
                    datas[-1].append(np.genfromtxt(f, comments='%',
                                                   dtype=str))
                    datas[-1] = np.array(datas[-1])
                    datas[-1] = np.transpose(datas[-1])

                    longitudes[-1] = datas[-1][3].astype(np.float)
                    latitudes[-1] = datas[-1][2].astype(np.float)
                    Q_factors[-1] = datas[-1][5].astype(np.float)

                    # the rtk quality of each file is calculated

                    Q_factors_counter = Q_factors[-1][:, 0]
                    count = Counter(Q_factors_counter)

                    # Counter function counts the amount of same numbers inside
                    # an array
                    # Example: Counter([1 1 2 3 2 4 6]) = [1:2 2:2 3:1 4:1 6:1]

                    Q1 = int(count[1])
                    Q2 = int(count[2])
                    Q5 = int(count[5])

                    if (Q1 + Q2 + Q5) != 0:
                        qual_1 = "fix: " +
                        str(float(format(Q1 * 100.0 / float(Q1 + Q2 + Q5),
                            '.2f')))
                        qual_2 = "float: " +
                        str(float(format(Q2 * 100.0 / float(Q1 + Q2 + Q5),
                            '.2f')))
                        qual_3 = "single: " +
                        str(float(format(Q5 * 100.0 / float(Q1 + Q2 + Q5),
                            '.2f')))

                        quality_percentage[-1].append(qual_1)
                        quality_percentage[-1].append(qual_2)
                        quality_percentage[-1].append(qual_3)

                    else:
                        quality_percentage[-1].append("fix: 0")
                        quality_percentage[-1].append("float: 0")
                        quality_percentage[-1].append("single: 0")

                except:
                    continue

    output_filename = raw_input("Name of the output .html file: ")
    output_filename = output_filename + ".html"

    # determining laps

    # The 'laps' list is as long as the coordinate list
    # so that every coordinate has a lap value.

    laps = []
    latdiff = 4e-4
    longdiff = 4e-4
    startlat = []
    startlong = []
    min_lat = latitudes[0][0]
    max_lat = latitudes[0][0]
    min_long = longitudes[0][0]
    max_long = longitudes[0][0]

    for f, name in zip(range(len(latitudes)), filenames):

        lap_amount = 0
        roundchange = False
        laps.append([])
        startlat.append(latitudes[f][0])
        startlong.append(longitudes[f][0])

        for l in range(len(latitudes[f])):
            if (abs(latitudes[f][l] - float(startlat[f]))
               < latdiff) and (abs(longitudes[f][l] - float(startlong[f]))
               < longdiff):

                if roundchange == 0:
                    lap_amount += 1
                    roundchange = True
            else:
                if roundchange == 1:
                    roundchange = False
            laps[-1].append(lap_amount)

            # maximum and minimum latitude and longitude for the grid

            if latitudes[f][l] >= max_lat:
                max_lat = latitudes[f][l]
            if latitudes[f][l] <= min_lat:
                min_lat = latitudes[f][l]
            if longitudes[f][l] >= max_long:
                max_long = longitudes[f][l]
            if longitudes[f][l] <= min_long:
                min_long = longitudes[f][l]

    max_min_diff_lat = max_lat - min_lat
    max_min_diff_long = max_long - min_long

    laps = np.array(laps)

    # average of startlats and startlongs for the initial position of the map:

    average_startlat = 0
    average_startlong = 0

    for ii in range(len(startlat)):

        average_startlat = average_startlat + startlat[ii]
        average_startlong = average_startlong + startlong[ii]

    average_startlat = average_startlat / (ii + 1)
    average_startlong = average_startlong / (ii + 1)

    # asign a color to each lap

    lapcolors = []

    for lap in Q_factors:
        lapcolors.append([])
        for Q in lap:

            if int(Q) == 1:
                lapcolors[-1].append('#00FF00')
            elif int(Q) == 2:
                lapcolors[-1].append('#FFFF00')
            elif int(Q) == 5:
                lapcolors[-1].append('#FF0000')
            else:
                lapcolors[-1].append('#000000')

    # main loop:

    while(True):

        html_data_flag = 0
        printed_laps, flag = options(printed_laps, laps)

        if flag == -1:
            break

        if flag == 0:
            html_data_flag = printLaps(latitudes, longitudes,
                                       average_startlat, average_startlong,
                                       lapcolors, printed_laps, html_data_flag)
            show_file = raw_input("open file (y/n) ")

            if show_file == "y":
                subprocess.Popen(['xdg-open', output_filename])
