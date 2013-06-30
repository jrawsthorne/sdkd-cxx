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

SET(COMMON_SOURCES
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

SET(LD_DEPS)

IF(${WIN32})
    LIST(APPEND COMMON_SOURCES
        win32-gettimeofday
        ThreadWin32.cpp)
    LIST(APPEND LD_DEPS libcouchbase ws2_32.lib)

ELSEIF(${UNIX})
    LIST(APPEND COMMON_SOURCES
        ThreadUnix.cpp)
    LIST(APPEND LD_DEPS rt couchbase)
    SET(LD_DEPS ${LD_DEPS} rt couchbase)
ENDIF()

ADD_EXECUTABLE(sdkd_lcb ${COMMON_SOURCES} sdkd_lcb.cpp)
TARGET_LINK_LIBRARIES(sdkd_lcb ${LD_DEPS})


INSTALL(TARGETS sdkd_lcb RUNTIME DESTINATION bin)
