cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(ytoncmd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_STR "v1.1")

# ====================================================================
# 1. FFmpeg 依賴偵測區（區分 Windows 與 類Unix 系統）
# ====================================================================

option(YTONCMD_WINDOWS_STATIC_FFMPEG "Link Windows/MSVC against the experimental static FFmpeg package" OFF)

if(WIN32)
    if(MSVC)
        if(YTONCMD_WINDOWS_STATIC_FFMPEG)
            message(STATUS "Detected Windows Native MSVC environment. Setting up STATIC FFmpeg paths.")
            set(FFMPEG_WIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-static")
        else()
            message(STATUS "Detected Windows Native MSVC environment. Setting up SHARED FFmpeg paths.")
            set(FFMPEG_WIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-shared")
        endif()

        if(NOT EXISTS "${FFMPEG_WIN_DIR}/include" OR NOT EXISTS "${FFMPEG_WIN_DIR}/lib")
            message(FATAL_ERROR "Windows FFmpeg package not found. Expected include/ and lib/ under: ${FFMPEG_WIN_DIR}")
        endif()

        if(YTONCMD_WINDOWS_STATIC_FFMPEG)
            # Static package from:
            # https://github.com/System233/ffmpeg-msvc-prebuilt/releases/tag/n7.1-241205
            set(FFMPEG_WIN_LIBS
                "${FFMPEG_WIN_DIR}/lib/libavformat.a"
                "${FFMPEG_WIN_DIR}/lib/libavcodec.a"
                "${FFMPEG_WIN_DIR}/lib/libswscale.a"
                "${FFMPEG_WIN_DIR}/lib/libswresample.a"
                "${FFMPEG_WIN_DIR}/lib/libavutil.a"
            )
            file(GLOB FFMPEG_WIN_DEP_LIBS CONFIGURE_DEPENDS "${FFMPEG_WIN_DIR}/lib/*.lib")
            list(APPEND FFMPEG_WIN_LIBS ${FFMPEG_WIN_DEP_LIBS})
        else()
            set(FFMPEG_WIN_LIBS
                "${FFMPEG_WIN_DIR}/lib/avformat.lib"
                "${FFMPEG_WIN_DIR}/lib/avcodec.lib"
                "${FFMPEG_WIN_DIR}/lib/swscale.lib"
                "${FFMPEG_WIN_DIR}/lib/swresample.lib"
                "${FFMPEG_WIN_DIR}/lib/avutil.lib"
            )
        endif()
        foreach(FFMPEG_WIN_LIB IN LISTS FFMPEG_WIN_LIBS)
            if(NOT EXISTS "${FFMPEG_WIN_LIB}")
                message(FATAL_ERROR "Required Windows FFmpeg library not found: ${FFMPEG_WIN_LIB}")
            endif()
        endforeach()

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
        target_link_libraries(ytoncmd PRIVATE ${FFMPEG_WIN_LIBS} ${FFMPEG_WIN_SYSTEM_LIBS})
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
    if(MSVC AND NOT YTONCMD_WINDOWS_STATIC_FFMPEG)
        set(FFMPEG_WIN_BIN_DIR "${FFMPEG_WIN_DIR}/bin")
        if(NOT EXISTS "${FFMPEG_WIN_BIN_DIR}")
            message(FATAL_ERROR "Windows shared FFmpeg bin directory not found: ${FFMPEG_WIN_BIN_DIR}")
        endif()
        add_custom_command(TARGET ytoncmd POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ytoncmd>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${FFMPEG_WIN_BIN_DIR}" "$<TARGET_FILE_DIR:ytoncmd>"
            COMMENT "Copying Windows FFmpeg shared DLLs to output directory..."
        )
    endif()
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
