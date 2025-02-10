cmake_minimum_required(VERSION 3.3.0 FATAL_ERROR)

if (POLICY CMP0071)
  cmake_policy(SET CMP0071 NEW)
endif ()

project(lafdup)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../lafrpc/cpp/lafrpc.h")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lafrpc/cpp/ lafrpc)
else()
    add_subdirectory(lafrpc/cpp lafrpc)
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
endif()

add_executable(lafdup WIN32 ${LAFDUP_SRC}
    lafdupapplication.h lafdupapplication.cpp
    )
target_link_libraries(lafdup Qt5::Widgets Qt5::Core lafrpc)

#查找Qt翻译工具
find_package(Qt5 REQUIRED COMPONENTS LinguistTools REQUIRED)

# 初始化要使用的列�?
set(TS_FILES
    translations/lafdup_zh_CN.ts
)
set(QM_FILES)

# 设置翻译文件的生成路径，如果不指定就会生成在CMakeFiles的目录里
set_source_files_properties(${TS_FILES}
    PROPERTIES OUTPUT_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/translations
)
# 创建翻译的最关键一�?
qt5_create_translation(QM_FILES ${TS_FILES} ${LAFDUP_SRC} OPTIONS -no-obsolete -no-ui-lines)

# 添加更新翻译的目�?
add_custom_target(lupdate_task DEPENDS ${TS_FILES})
add_custom_target(lrelease_task DEPENDS ${QM_FILES})
