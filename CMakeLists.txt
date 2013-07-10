cmake_minimum_required(VERSION 2.4)
project(sdkd_cpp)

IF(NOT CMAKE_INSTALL_PREFIX)
    SET(DESTDIR inst)
    SET(CMAKE_INSTALL_PREFIX inst)
ENDIF()

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
add_subdirectory(src)
