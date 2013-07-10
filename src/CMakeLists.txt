cmake_minimum_required(VERSION 2.8)

set(CONTRIB_ROOT contrib)
set(JSONCPP_ROOT ${CONTRIB_ROOT}/json-cpp)
set(JSONSL_ROOT ${CONTRIB_ROOT}/jsonsl)
set(CLIOPTS_ROOT ${CONTRIB_ROOT}/cliopts)

INCLUDE_DIRECTORIES(
    ${SOURCE_ROOT}
    ${JSONSL_ROOT}
    ${JSONCPP_ROOT}/dist
    ${CLIOPTS_ROOT}
    ${LCB_ROOT}/include)

LINK_DIRECTORIES(${LCB_ROOT}/lib)

IF(NOT LCB_LIBNAME)
    IF(${WIN32})
        SET(LCB_LIBNAME "libcouchbase")
    ELSE()
        SET(LCB_LIBNAME "couchbase")
    ENDIF(${WIN32})

ENDIF(NOT LCB_LIBNAME)


SET(COMMON_SOURCES
    Daemon.cpp
    Message.cpp
    Request.cpp
    Dataset.cpp
    Response.cpp
    Handle.cpp
    ResultSet.cpp
    IODispatch.cpp
    Worker.cpp

    ViewExecutor.cpp
    ViewLoader.cpp
    views/viewrow.c
    views/viewopts.c

    sockutil.c

    ${CONTRIB_ROOT}/debug.c
    ${JSONSL_ROOT}/jsonsl.c
    ${CLIOPTS_ROOT}/cliopts.c
    ${JSONCPP_ROOT}/dist/jsoncpp.cpp)

IF(${WIN32})
    LIST(APPEND COMMON_SOURCES
        win32-gettimeofday
        ThreadWin32.cpp)

    LIST(APPEND LD_DEPS
        ${LCB_LIBNAME}
        ws2_32.lib)

    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)

ELSEIF(${UNIX})
    LIST(APPEND COMMON_SOURCES
        ThreadUnix.cpp)

    LIST(APPEND LD_DEPS
        pthread
        ${LCB_LIBNAME})

    IF(GPROF)
        LIST(APPEND LD_DEPS
            profiler)
    ENDIF(GPROF)

    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++98 ${SDKD_PERF}")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall ${SDKD_PERF}")

ENDIF()

ADD_EXECUTABLE(sdkd_lcb ${COMMON_SOURCES} Main.cpp)
TARGET_LINK_LIBRARIES(sdkd_lcb ${LD_DEPS})


INSTALL(TARGETS sdkd_lcb RUNTIME DESTINATION bin)
