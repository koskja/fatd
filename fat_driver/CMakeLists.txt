cmake_minimum_required(VERSION 3.0.0)
project(FatDriver VERSION 0.1.0)

include(CTest)
enable_testing()

cmake_policy(SET CMP0063 NEW)
add_library(FatDriver SHARED fat.c file.c cache.c)
set_target_properties(FatDriver PROPERTIES C_VISIBILITY_PRESET hidden)

add_subdirectory(utf8-utf16-converter)

target_link_libraries(FatDriver PUBLIC converter)
target_include_directories(FatDriver PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/utf8-utf16-converter"
                          )

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})


include(CPack)
