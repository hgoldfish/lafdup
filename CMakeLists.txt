cmake_minimum_required(VERSION 3.3.0 FATAL_ERROR)

if (POLICY CMP0071)
  cmake_policy(SET CMP0071 NEW)
endif ()

project(lafdup)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../lafrpc/cpp/lafrpc.h")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lafrpc/cpp/ lafrpc)
else()
    add_subdirectory(lafrpc)
endif()

set(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)

set(LAFDUP_SRC
    main.cpp
    lafdup_window.cpp
    lafdup_window.h
    lafdup_window_p.h
    lafdup.qrc

    peer.h
    peer_p.h
    discovery.h
    models.h

    peer.cpp
    discovery.cpp
    models.cpp

    main.ui
    password.ui
    configure.ui
)

add_definitions(-DQT_NO_CAST_TO_ASCII)
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -z noexecstack")
endif()

add_executable(lafdup WIN32 ${LAFDUP_SRC})
target_link_libraries(lafdup Qt5::Widgets Qt5::Core lafrpc)

