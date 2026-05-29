# Patches yaml-cpp's CMakeLists.txt for CMake 4.x compatibility.
# yaml-cpp 0.8.0 declares cmake_minimum_required(VERSION 2.8.12) which CMake 4.x
# no longer accepts (minimum allowed is 3.5).
#
# Invoked from FetchContent PATCH_COMMAND as:
#   cmake -DFILE=<SOURCE_DIR>/CMakeLists.txt -P this_script.cmake
#
# FILE must be passed as an absolute path because cmake -P resolves relative
# paths in file() against CMAKE_CURRENT_SOURCE_DIR (the script's own directory),
# not the process working directory.
cmake_minimum_required(VERSION 3.5)
if(NOT DEFINED FILE)
    message(FATAL_ERROR "patch_yaml_cpp.cmake: FILE variable not set")
endif()
file(READ "${FILE}" _content)
string(REPLACE
    "cmake_minimum_required(VERSION 2.8.12)"
    "cmake_minimum_required(VERSION 3.5)"
    _content "${_content}")
file(WRITE "${FILE}" "${_content}")
