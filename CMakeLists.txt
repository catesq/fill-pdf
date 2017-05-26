cmake_minimum_required(VERSION 2.6)
include(ExternalProject)

STRING(TOLOWER "${CMAKE_BUILD_TYPE}" MUPDF_BUILD)
IF(NOT "${MUPDF_BUILD}" STREQUAL "debug")
    SET(MUPDF_BUILD "release")
ENDIF()

EXTERNALPROJECT_ADD(mupdf
    URL               "http://mupdf.com/downloads/archive/mupdf-1.10a-source.tar.gz"
    PREFIX            "${CMAKE_CURRENT_BINARY_DIR}/mupdf"
    SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/mupdf"
    STAMP_DIR         "${CMAKE_CURRENT_BINARY_DIR}/mupdf-stamp"
    TMP_DIR           "${CMAKE_CURRENT_BINARY_DIR}/mupdf-tmp"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     make ${MUPDF_BUILD}
    INSTALL_COMMAND   ""
    BUILD_IN_SOURCE   1
)

PROJECT(fillpdf)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/mupdf/include)

ADD_EXECUTABLE(fillpdf fill_cli.c map_input.c parse.c util.c complete.c)
ADD_DEPENDENCIES(fillpdf mupdf)

SET(MUPDF_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/mupdf/build/${MUPDF_BUILD}")

TARGET_LINK_LIBRARIES(fillpdf "${MUPDF_LIB_DIR}/libcurl.a" "${MUPDF_LIB_DIR}/libmupdf.a" "${MUPDF_LIB_DIR}/libmupdfthird.a" jansson z m ssl crypto)
