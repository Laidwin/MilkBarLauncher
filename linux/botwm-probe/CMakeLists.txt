cmake_minimum_required(VERSION 3.16)
project(botwm_probe CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Threads REQUIRED)

add_library(botwm_probe SHARED probe.cpp)
target_compile_options(botwm_probe PRIVATE -Wall -Wextra -fvisibility=hidden)
target_link_libraries(botwm_probe PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
