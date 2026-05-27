cmake_minimum_required(VERSION 3.10)
project(ytoncmd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_STR "v1.1")

# ====================================================================
# 1. FFmpeg 依賴偵測區（區分 Windows 與 類Unix 系統）
# ====================================================================

# 這裡使用https://github.com/System233/ffmpeg-msvc-prebuilt/releases/tag/n7.1-241205 編譯的靜態庫
if(WIN32)
    if(MSVC)
        message(STATUS "Detected Windows Native MSVC environment. Setting up STATIC FFmpeg paths.")
        
        # 💡 完美對齊：指向剛才在 YAML 裡改名的 ffmpeg-static 資料夾
        set(FFMPEG_WIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-static")
        
        # 指定 Windows 尋找 .lib 檔的目錄
        link_directories("${FFMPEG_WIN_DIR}/lib")
        
        # 指定原生 Windows MSVC 點名要連結的標準 .lib 靜態導入庫與系統硬體加速庫
        set(FFMPEG_WIN_LIBS 
            # 1. FFmpeg 核心五大金剛
            "${FFMPEG_WIN_DIR}/lib/libavformat.a"
            "${FFMPEG_WIN_DIR}/lib/libavcodec.a"
            "${FFMPEG_WIN_DIR}/lib/libavutil.a"
            "${FFMPEG_WIN_DIR}/lib/libswscale.a"
            "${FFMPEG_WIN_DIR}/lib/libswresample.a"
            
            # 2. System233 同資料夾附帶的次級依賴靜態庫（解決 133 個未解析錯誤）
            "${FFMPEG_WIN_DIR}/lib/libx264.lib"
            "${FFMPEG_WIN_DIR}/lib/x265-static.lib"
            "${FFMPEG_WIN_DIR}/lib/zlib.lib"
            "${FFMPEG_WIN_DIR}/lib/libwebp.lib"
            "${FFMPEG_WIN_DIR}/lib/jxl.lib"
            "${FFMPEG_WIN_DIR}/lib/jxl_threads.lib"
            "${FFMPEG_WIN_DIR}/lib/vpxmt.lib"
            "${FFMPEG_WIN_DIR}/lib/libsharpyuv.lib"
            "${FFMPEG_WIN_DIR}/lib/hwy.lib"
            "${FFMPEG_WIN_DIR}/lib/brotlicommon.lib"
            "${FFMPEG_WIN_DIR}/lib/brotlidec.lib"
            "${FFMPEG_WIN_DIR}/lib/brotlienc.lib"
            
            # 3. Windows 原生系統底層必備庫（ws2_32 負責解決通訊、其餘負責硬體加速與安全）
            ws2_32 Bcrypt Secur32 Strmiids Vfw32 Shlwapi Mfuuid
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
    target_compile_definitions(ytoncmd PRIVATE NOMINMAX)
endif()

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