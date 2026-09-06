cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

SET(LIBPSD_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_psd.c
        ${CMAKE_CURRENT_SOURCE_DIR}/psd_decode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/psd_stubs.cpp
)

SET(LIBPSD_LIBS
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libpsd.a
)

SET(LIBPSD_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_filter(libpsd
        "${LIBPSD_SRC}"
        "${LIBPSD_LIBS}"
        []
        ""
        "${LIBPSD_INC}"
        ""
        "1")

# psd_sdk is built as C++17 (its headers use inline variables and if-constexpr),
# which is emcc's default, but the standard is pinned here so the filter cannot
# drift away from the archive it links against.
set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/psd_decode.cpp
        PROPERTIES COMPILE_FLAGS "-std=c++17")
