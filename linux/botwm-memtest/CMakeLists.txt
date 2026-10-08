cmake_minimum_required(VERSION 3.16)
project(botwm_memtest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(DLL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../DLL/InjectDLL)

find_package(Threads REQUIRED)

add_library(botwm_memtest SHARED
    memtest.cpp
    ${DLL_DIR}/Platform.cpp
    ${DLL_DIR}/Scanner.cpp
    ${DLL_DIR}/ReadCemu.cpp
    ${DLL_DIR}/LoggerService.cpp
)
target_include_directories(botwm_memtest PRIVATE ${DLL_DIR} ${DLL_DIR}/include)
target_compile_options(botwm_memtest PRIVATE -fvisibility=hidden)
target_link_libraries(botwm_memtest PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
