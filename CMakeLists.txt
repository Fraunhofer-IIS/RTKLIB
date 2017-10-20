project(rtklib)
cmake_minimum_required(VERSION 2.8)

add_custom_target(iers ALL COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/lib/iers/gcc)

set(app_list str2str pos2kml rtkrcv rnx2rtkp convbin)
foreach(app ${app_list})
    set(dir app/${app}/gcc)
    message(STATUS "building ${app} in ${dir}")
    set(absolute_dir ${CMAKE_CURRENT_SOURCE_DIR}/${dir})
    add_custom_target(${app} ALL
        COMMAND ${CMAKE_MAKE_PROGRAM} -C ${absolute_dir}
        COMMENT "Building ${app} in ${dir}"
    )
    add_dependencies(${app} iers)
    add_custom_command(TARGET ${app} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${absolute_dir}/${app} ${CMAKE_BINARY_DIR}
        COMMENT "Copy ${app} to build directory"
    )
    install(PROGRAMS ${CMAKE_BINARY_DIR}/${app} DESTINATION bin)
endforeach()
