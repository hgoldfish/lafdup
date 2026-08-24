# MinGW-w64 i686 toolchain targeting Windows XP (5.1) with a statically linked Qt 5.6.3.
# The Qt prefix (/opt/qt-winxp) is produced by pc/Dockerfile.winxp.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
set(CMAKE_ADDR2LINE i686-w64-mingw32-addr2line)
set(CMAKE_AR i686-w64-mingw32-ar)
set(CMAKE_NM i686-w64-mingw32-nm)
set(CMAKE_OBJCOPY i686-w64-mingw32-objcopy)
set(CMAKE_OBJDUMP i686-w64-mingw32-objdump)
set(CMAKE_RANLIB i686-w64-mingw32-ranlib)
set(CMAKE_STRIP i686-w64-mingw32-strip)

set(LAFDUP_QT_PREFIX "/opt/qt-winxp" CACHE PATH "Cross-compiled Qt 5.6.3 prefix")
set(CMAKE_FIND_ROOT_PATH ${LAFDUP_QT_PREFIX} /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(LAFDUP_MINGW_XP_COMPAT ON CACHE BOOL "Link XP API shims for mingw" FORCE)
set(LAFDUP_XP_WINVER "0x0501" CACHE STRING "WINVER for lafdup XP builds" FORCE)
set(LAFDUP_XP_WINNT "0x0501" CACHE STRING "_WIN32_WINNT for lafdup XP builds" FORCE)

# -static pulls in libwinpthread.a (posix model) so no libwinpthread-1.dll at runtime.
# pc/cmake/xp-compat/xp_api_shims.c supplies GetTickCount64/inet_pton import thunks missing on XP.
set(_XP_LINK_FLAGS
    "-static -Wl,--major-os-version,5 -Wl,--minor-os-version,1 -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,1"
)
set(CMAKE_EXE_LINKER_FLAGS "${_XP_LINK_FLAGS}" CACHE STRING "XP exe linker flags" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${_XP_LINK_FLAGS}" CACHE STRING "XP shared linker flags" FORCE)
set(CMAKE_MODULE_LINKER_FLAGS "${_XP_LINK_FLAGS}" CACHE STRING "XP module linker flags" FORCE)
