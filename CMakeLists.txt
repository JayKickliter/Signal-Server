##########################################################################
# Project setup                                                          #
##########################################################################
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(SignalServer LANGUAGES CXX)

# ISO standard compliance (C++14)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


##########################################################################
# Clang tools                                                            #
##########################################################################

# get all project files
file(GLOB_RECURSE ALL_SOURCE_FILES *.cc *.hh)

add_custom_target(
  format
  COMMAND clang-format
  -i
  ${ALL_SOURCE_FILES}
)

add_custom_target(
  check-format
  COMMAND clang-format
  --dry-run
  --Werror
  ${ALL_SOURCE_FILES}
)

##########################################################################
# Common external libraries                                              #
##########################################################################
find_library(bz2 bz2)
find_library(dl dl)
find_library(pthread pthread)
find_library(z z)
find_library(png png)


##########################################################################
# Subprojects                                                            #
##########################################################################

# Main applicaiton(s)
add_subdirectory(src)
