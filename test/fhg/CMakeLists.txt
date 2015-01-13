project(rtklib)
cmake_minimum_required(VERSION 2.8)

add_subdirectory(${CMAKE_SOURCE_DIR}/../.. rtklib)

enable_testing()
add_test(novatel_position sh -c "IN_PATH=${CMAKE_SOURCE_DIR}/data ${CMAKE_SOURCE_DIR}/position_test.sh")
add_test(ogrp_position sh -c "IN_PATH=${CMAKE_SOURCE_DIR}/data LOG_NAME=ogrp EXTENSION=.ogrp FORMAT=ogrp ${CMAKE_SOURCE_DIR}/position_test.sh")
set_tests_properties(ogrp_position PROPERTIES PASS_REGULAR_EXPRESSION "decode_ogrp_ch_meas: GNSS/signal combination is not supported")
