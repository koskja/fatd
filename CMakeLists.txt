cmake_minimum_required(VERSION 3.0.0)
project(fatd VERSION 0.1.0)

include(CTest)
enable_testing()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(fat_driver)

add_executable(fatd main.c)

target_link_libraries(fatd PUBLIC FatDriver)
target_include_directories(fatd PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/fat_driver"
                          )

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
