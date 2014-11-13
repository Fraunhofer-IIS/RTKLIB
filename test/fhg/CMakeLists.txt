project(rtklib)
cmake_minimum_required(VERSION 2.8)

add_subdirectory(${CMAKE_SOURCE_DIR}/../.. rtklib)

enable_testing()
add_test(novatel_position sh -c "IN_PATH=${CMAKE_SOURCE_DIR}/data ${CMAKE_SOURCE_DIR}/position_test.sh")
