cmake_minimum_required(VERSION 3.20)

project(GrfxTechniques)

set(CMAKE_CXX_STANDARD 20)

set(WIL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/wil)

add_subdirectory(external/glm)

add_subdirectory(cmake)
add_subdirectory(src)
