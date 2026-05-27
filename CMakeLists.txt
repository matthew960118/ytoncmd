cmake_minimum_required(VERSION 3.10)
project(ytoncmd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_STR "v1.1")

# ====================================================================
# 1. FFmpeg 依賴偵測區（區分 Windows 與 類Unix 系統）
# ====================================================================
if(WIN32)
    if(MSVC)
        message(STATUS "Detected Windows Native MSVC environment. Setting up local FFmpeg paths.")
        set(FFMPEG_WIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-8.1.1-full_build-shared")
        link_directories("${FFMPEG_WIN_DIR}/lib")
        set(FFMPEG_WIN_LIBS avformat avcodec avutil swscale swresample)
    else()
        message(STATUS "Detected Windows Cross-Compilation (MinGW). Using environment variables.")
        include_directories(include third_party)
    endif()
else()
    # 類 Unix 系統分流：Linux 絕對不碰 STATIC 關鍵字
    find_package(PkgConfig REQUIRED)
    if(APPLE)
        message(STATUS "Detected macOS target. Enabling STATIC link for FFmpeg.")
        pkg_check_modules(FFMPEG STATIC REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
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

# ====================================================================
# 3. 現代 Target 屬性設定（標頭檔與連結庫）
# ====================================================================
if(WIN32)
    if(MSVC)
        target_include_directories(ytoncmd PRIVATE include third_party "${FFMPEG_WIN_DIR}/include")
        target_link_libraries(ytoncmd PRIVATE ${FFMPEG_WIN_LIBS})
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
    
    set(FFMPEG_BIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-8.1.1-full_build-shared/bin")
    if(EXISTS "${FFMPEG_BIN_DIR}")
        add_custom_command(TARGET ytoncmd POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WIN_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${FFMPEG_BIN_DIR}" "${WIN_OUTPUT_DIR}"
            COMMENT "Copying Windows FFmpeg DLLs and executables to output directory..."
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