cmake_minimum_required(VERSION 3.3.0 FATAL_ERROR)

project(lafdup)

set(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTOUIC ON)
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
)

qt5_add_resources(RESOURCE_SOURCES lafdup.qrc)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qtnetworkng/qtnetworkng.h")
    add_subdirectory(qtnetworkng)
else()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../qtnetworkng/ qtnetworkng)
endif()

add_executable(lafdup WIN32 ${LAFDUP_SRC} ${RESOURCE_SOURCES})
target_link_libraries(lafdup Qt5::Widgets Qt5::Core qtnetworkng)

