cmake_minimum_required(VERSION 3.3.0 FATAL_ERROR)

if (POLICY CMP0071)
  cmake_policy(SET CMP0071 NEW)
endif ()

project(lafdup)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../lafrpc/cpp/lafrpc.h")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lafrpc/cpp/ lafrpc)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../lafrpc/cpp/lafrpc.h")
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
    lafdupapplication.h
    lafdupapplication.cpp
    lafdup.qrc
    images/logo.rc

    peer.h
    peer_p.h
    discovery.h
    discovery_p.h
    models.h

    peer.cpp
    discovery.cpp
    models.cpp

    main.ui
    password.ui
    configure.ui
    guide.ui
)

add_definitions(-DQT_NO_CAST_TO_ASCII)
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

# Fix Qt-static cmake BUG
# https://bugreports.qt.io/browse/QTBUG-38913
# FIXME qt 5.13 fix this bug
if(EXISTS ${_qt5Core_install_prefix}/lib/libQt5Core.a AND NOT EXISTS ${_qt5Core_install_prefix}/bin/Qt5Core.dll)
    message("Static Qt is detected.")
    link_directories(${_qt5Core_install_prefix}/lib/)
endif()

# MinGW XP toolchains: avoid Vista+ imports without patching LibreSSL sources.
# - pthread.h: prepend our include dir so LibreSSL TUs see XP pthread_once
# - getentropy: provide our object first so libcrypto's BCrypt getentropy is not pulled
# - inet_pton / GetTickCount64: local import thunks
set(LAFDUP_XP_SHIMS)
if(MINGW AND LAFDUP_MINGW_XP_COMPAT)
    enable_language(C)
    set(_LAFDUP_XP_COMPAT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/xp-compat")

    # Prefer our pthread.h over LibreSSL include/compat (no file copies into libressl/).
    foreach(_t crypto_obj ssl_obj crypto ssl)
        if(TARGET ${_t})
            target_include_directories(${_t} BEFORE PRIVATE "${_LAFDUP_XP_COMPAT_DIR}")
        endif()
    endforeach()

    # qtng/lafrpc/lafdup use XP WINNT; LibreSSL keeps its own 0x0600 (separate TUs, no -D clash).
    foreach(_t qtng qtnetworkng lafrpc)
        if(TARGET ${_t})
            target_compile_definitions(${_t} PRIVATE
                WINVER=${LAFDUP_XP_WINVER}
                _WIN32_WINNT=${LAFDUP_XP_WINNT})
        endif()
    endforeach()

    add_library(lafdup_xp_compat OBJECT
        ${_LAFDUP_XP_COMPAT_DIR}/xp_api_shims.c
        ${_LAFDUP_XP_COMPAT_DIR}/getentropy_win.c)
    target_include_directories(lafdup_xp_compat PRIVATE "${_LAFDUP_XP_COMPAT_DIR}")
    target_compile_definitions(lafdup_xp_compat PRIVATE
        WINVER=${LAFDUP_XP_WINVER}
        _WIN32_WINNT=${LAFDUP_XP_WINNT})
    target_link_libraries(lafdup_xp_compat PUBLIC ws2_32 user32 advapi32)

    # OBJECT sources first so local getentropy / __imp_* thunks win over archive/DLL imports.
    set(LAFDUP_XP_SHIMS $<TARGET_OBJECTS:lafdup_xp_compat>)
endif()

add_executable(lafdup WIN32 ${LAFDUP_SRC})
target_link_libraries(lafdup Qt5::Widgets Qt5::Core lafrpc ${LAFDUP_XP_SHIMS})
# Qt 静态链接时的私有 3rdparty 依赖（pcre/harfbuzz），见 Qt5Core/Gui.prl。
# pcre 是 Qt5Core 的依赖、harfbuzz 是 Qt5Gui 的依赖；
# 挂在各自模块的 INTERFACE 上，确保链接顺序紧跟对应模块之后。
if(EXISTS ${_qt5Core_install_prefix}/lib/libqtpcre.a)
    target_link_libraries(Qt5::Core INTERFACE ${_qt5Core_install_prefix}/lib/libqtpcre.a)
endif()
if(EXISTS ${_qt5Core_install_prefix}/lib/libqtharfbuzzng.a)
    target_link_libraries(Qt5::Gui INTERFACE ${_qt5Core_install_prefix}/lib/libqtharfbuzzng.a)
endif()
if(LAFDUP_XP_SHIMS)
    target_compile_definitions(lafdup PRIVATE
        WINVER=${LAFDUP_XP_WINVER}
        _WIN32_WINNT=${LAFDUP_XP_WINNT})
    # qAsConst() 是 Qt 5.7+ API；XP 构建强制注入兼容实现。
    # 拆成两个参数，避免 CMake 将 "-include <path>" 合并为带引号的单参数。
    target_compile_options(lafdup PRIVATE "-include" "${CMAKE_CURRENT_SOURCE_DIR}/compat/qasconst.h")
endif()

#查找Qt翻译工具
find_package(Qt5 COMPONENTS LinguistTools QUIET)

# 初始化要使用的列表
set(TS_FILES
    translations/lafdup_zh_CN.ts
)
set(QM_FILES)

# 只有在找到LinguistTools时才进行翻译相关的工作
if(Qt5LinguistTools_FOUND)
    # 设置翻译文件的生成路径，如果不指定就会生成在CMakeFiles的目录里
    set_source_files_properties(${TS_FILES}
        PROPERTIES OUTPUT_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/translations
    )
    # 创建翻译的最关键一步
    qt5_create_translation(QM_FILES ${TS_FILES} ${LAFDUP_SRC} OPTIONS -no-obsolete -no-ui-lines)

    # 添加更新翻译的目标
    add_custom_target(lupdate_task DEPENDS ${TS_FILES})
    add_custom_target(lrelease_task DEPENDS ${QM_FILES})
endif()


