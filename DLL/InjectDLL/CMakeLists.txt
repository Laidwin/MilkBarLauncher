# Linux build of the mod as libbotwm.so, loaded into native Cemu with LD_PRELOAD.
# Windows keeps using InjectDLL.vcxproj; keep both source lists in sync.
cmake_minimum_required(VERSION 3.16)
project(botwm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Threads REQUIRED)

add_library(botwm SHARED
    BombSyncer.cpp
    CharacterClasses.cpp
    Client.cpp
    Deserializer.cpp
    dllmain.cpp
    dllmain_Functions.cpp
    InterpolationFunctions.cpp
    Interpretation.cpp
    LoggerService.cpp
    MessagerService.cpp
    multiplayerQuest.cpp
    NamedPipes.cpp
    Platform.cpp
    Player.cpp
    QuaternionBE.cpp
    QuestSync.cpp
    ReadCemu.cpp
    Scanner.cpp
    Serialization.cpp
    Serializer.cpp
    Spawning.cpp
    SpawningFunction.cpp
    Vec3fBE.cpp
    WorldClass.cpp
)
target_include_directories(botwm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(botwm PRIVATE RAPIDJSON_HAS_STDSTRING=1)
target_compile_options(botwm PRIVATE -fvisibility=hidden)
target_link_libraries(botwm PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
