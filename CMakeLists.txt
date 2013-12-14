cmake_minimum_required(VERSION 2.4)
project(sdkd_cpp)

IF(NOT CMAKE_INSTALL_PREFIX)
    SET(DESTDIR inst)
    SET(CMAKE_INSTALL_PREFIX inst)
ENDIF()

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(SOURCE_ROOT ${CMAKE_SOURCE_DIR})
set(CONTRIB_ROOT ${CMAKE_SOURCE_DIR}/src/contrib)
set(JSONCPP_ROOT ${CONTRIB_ROOT}/json-cpp)
set(JSONSL_ROOT ${CONTRIB_ROOT}/jsonsl)
set(CLIOPTS_ROOT ${CONTRIB_ROOT}/cliopts)

INCLUDE_DIRECTORIES(
    ${SOURCE_ROOT}
    ${JSONSL_ROOT}
    ${JSONCPP_ROOT}/dist
    ${CLIOPTS_ROOT}
    ${LCB_ROOT}/include)

LINK_DIRECTORIES(${LCB_ROOT}/lib ${LCB_ROOT}/bin)

IF(NOT LCB_LIBNAME)
    IF(${WIN32})
        SET(LCB_LIBNAME "libcouchbase")
    ELSE()
        SET(LCB_LIBNAME "couchbase")
    ENDIF(${WIN32})

ENDIF(NOT LCB_LIBNAME)


SET(COMMON_SOURCES
    src/Control.cpp
    src/Daemon.cpp
    src/Message.cpp
    src/Request.cpp
    src/Dataset.cpp
    src/Response.cpp
    src/Handle.cpp
    src/ResultSet.cpp
    src/IODispatch.cpp
    src/Worker.cpp

    src/ViewExecutor.cpp
    src/ViewLoader.cpp
    src/views/viewrow.c
    src/views/viewopts.c

    src/sockutil.c

    ${CONTRIB_ROOT}/debug.c
    ${JSONSL_ROOT}/jsonsl.c
    ${CLIOPTS_ROOT}/cliopts.c
    ${JSONCPP_ROOT}/dist/jsoncpp.cpp)

IF(${WIN32})
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    LIST(APPEND COMMON_SOURCES
        src/win32-gettimeofday
        src/ThreadWin32.cpp)

    LIST(APPEND LD_DEPS
        ${LCB_LIBNAME}
        ws2_32.lib)

    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-DWINVER=0x0501)
    IF(MSVC)
        SET(COMMON_FLAGS "/W3 /EHsc")
    ELSE()
        SET(COMMON_FLAGS "-gstabs -Wall")
    ENDIF()

ELSEIF(${UNIX})
    LIST(APPEND COMMON_SOURCES
        src/ThreadUnix.cpp)

    LIST(APPEND LD_DEPS
        pthread
        ${LCB_LIBNAME})

    IF(GPROF)
        LIST(APPEND LD_DEPS
            profiler)
    ENDIF(GPROF)
    SET(COMMON_FLAGS "-Wall ${SDKD_PERF}")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98")
ENDIF()

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")

ADD_EXECUTABLE(sdkd_lcb ${COMMON_SOURCES} src/Main.cpp)
ADD_EXECUTABLE(ds_test ${COMMON_SOURCES} src/DSTest.cpp)

TARGET_LINK_LIBRARIES(sdkd_lcb ${LD_DEPS})
TARGET_LINK_LIBRARIES(ds_test ${LD_DEPS})
IF(${WIN32})
    # Copy the DLL
    FOREACH(dllpath "${LCB_ROOT}/lib" "${LCB_ROOT}/bin")
        IF(EXISTS "${dllpath}/libcouchbase.dll")
            EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy
                "${dllpath}/libcouchbase.dll"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcouchbase.dll")
        ENDIF()
    ENDFOREACH()
ELSE()
    INSTALL(TARGETS sdkd_lcb RUNTIME DESTINATION bin)
ENDIF()
