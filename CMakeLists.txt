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
# Common external libraries                                              #
##########################################################################
find_library(bz2 bz2)
find_library(dl dl)
find_library(pthread pthread)
find_library(z z)


##########################################################################
# Subprojects                                                            #
##########################################################################

# Main applicaiton(s)
add_subdirectory(src)

# USGS to SDF utility
add_subdirectory(utils/sdf/usgs2sdf)
