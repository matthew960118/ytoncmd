cmake_minimum_required(VERSION 3.10)
project(ytoncmd)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_STR "v1.1")

# ====================================================================
# 1. FFmpeg 依賴偵測區（區分 Windows 與 類Unix 系統）
# ====================================================================

# Windows/MSVC uses the static FFmpeg package from:
# https://github.com/System233/ffmpeg-msvc-prebuilt/releases/tag/n7.1-241205
if(WIN32)
    if(MSVC)
        message(STATUS "Detected Windows Native MSVC environment. Setting up STATIC FFmpeg paths.")

        set(FFMPEG_WIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-static")

        if(NOT EXISTS "${FFMPEG_WIN_DIR}/include" OR NOT EXISTS "${FFMPEG_WIN_DIR}/lib")
            message(FATAL_ERROR "Windows static FFmpeg package not found. Expected include/ and lib/ under: ${FFMPEG_WIN_DIR}")
        endif()

        file(GLOB FFMPEG_WIN_STATIC_LIBS CONFIGURE_DEPENDS "${FFMPEG_WIN_DIR}/lib/*.lib")
        if(NOT FFMPEG_WIN_STATIC_LIBS)
            message(FATAL_ERROR "No FFmpeg static .lib files found in: ${FFMPEG_WIN_DIR}/lib")
        endif()

        set(FFMPEG_WIN_SYSTEM_LIBS
            ws2_32 Bcrypt Secur32 Strmiids Vfw32 Shlwapi Mfuuid d3d11 dxva2 ole32
            user32 advapi32 gdi32 shell32
        )
    else()
        message(STATUS "Detected Windows Cross-Compilation (MinGW). Using environment variables.")
        include_directories(include third_party)
    endif()
else()
    # 類 Unix 系統分流：Linux 絕對不碰 STATIC 關鍵字
    find_package(PkgConfig REQUIRED)
    if(APPLE)
        message(STATUS "Detected macOS target. Enabling STATIC link for FFmpeg.")
        set(PKG_CONFIG_ARGN "--static")
        pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
    else()
        message(STATUS "Detected Linux target. Enabling DYNAMIC link for FFmpeg.")
        pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
    endif()
endif()

# ====================================================================
# 2. 原始碼與執行檔設定
# ====================================================================
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp" "main.cpp")
add_executable(ytoncmd ${SOURCES})
if(WIN32)
    target_compile_definitions(ytoncmd PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        set_property(TARGET ytoncmd PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        set_target_properties(ytoncmd PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
    endif()
endif()

# ====================================================================
# 3. 現代 Target 屬性設定（標頭檔與連結庫）
# ====================================================================
if(WIN32)
    if(MSVC)
        target_include_directories(ytoncmd PRIVATE include third_party "${FFMPEG_WIN_DIR}/include")
        target_link_libraries(ytoncmd PRIVATE ${FFMPEG_WIN_STATIC_LIBS} ${FFMPEG_WIN_SYSTEM_LIBS})
    else()
        target_link_libraries(ytoncmd PRIVATE ${FFMPEG_LIBRARIES})
    endif()
else()
    # Linux & macOS 統一使用 PkgConfig 的虛擬 target，並且吃本地的 include 夾
    target_link_libraries(ytoncmd PRIVATE PkgConfig::FFMPEG)
    target_include_directories(ytoncmd PRIVATE include third_party)
endif()

# ====================================================================
# 4. 輸出路徑與各平台客製化動態庫處理
# ====================================================================
if(WIN32)
    set(WIN_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build_win/ytoncmd-win-${VERSION_STR}")
    set_target_properties(ytoncmd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${WIN_OUTPUT_DIR}")
else()
    if(APPLE)
        set(PLATFORM_NAME "macos")
    else()
        set(PLATFORM_NAME "linux")
    endif()

    set(UNIX_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/ytoncmd-${PLATFORM_NAME}-${VERSION_STR}")
    set_target_properties(ytoncmd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${UNIX_OUTPUT_DIR}")

    if(NOT APPLE)
        set_target_properties(ytoncmd PROPERTIES
            BUILD_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "$ORIGIN"
        )
    endif()
endif()