cmake_minimum_required(VERSION 3.3.0 FATAL_ERROR)

project(lafdup)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qtnetworkng/qtnetworkng.h")
    add_subdirectory(qtnetworkng)
else()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../qtnetworkng/ qtnetworkng)
endif()

if (POLICY CMP0071)
  cmake_policy(SET CMP0071 NEW)
endif ()

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
    lafdup_peer.h
    lafdup_peer.cpp
    lafdup.qrc
)

add_executable(lafdup WIN32 ${LAFDUP_SRC})
target_link_libraries(lafdup Qt5::Widgets Qt5::Core qtnetworkng)

