cmake_minimum_required(VERSION 3.10)
project(ytoncmd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_STR "v1.1")

# ====================================================================
# 1. FFmpeg 依賴偵測區（區分 Windows 與 類Unix 系統）
# ====================================================================
if(WIN32)
    message(STATUS "Detected Windows target, skipping Linux pkg-config.")
    # Windows 的路徑依賴你原本工具鏈檔案設定的 ${FFMPEG_LIBRARIES}
    # 這裡我們補上傳統的全域 include 確保 Windows 找得到 include 和 third_party
    include_directories(include third_party)
else()
    # 只有在 Linux 或 macOS (APPLE) 才啟用 PkgConfig
    find_package(PkgConfig REQUIRED)
    
    # 使用 pkg-config 一把抓，加上 IMPL_TARGETS 參數會讓它自動生成帶路徑的 target
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
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
    # Windows 連結原本工具鏈指定的庫
    target_link_libraries(ytoncmd PRIVATE ${FFMPEG_LIBRARIES})
else()
    # Linux & macOS 透過 PkgConfig 建立的虛擬 target 連結，它會自動把 .h 和 .so 完美綁定
    target_link_libraries(ytoncmd PRIVATE PkgConfig::FFMPEG)
    
    # 確保本地的 include 與 third_party 也能被類 Unix 系統吃到
    target_include_directories(ytoncmd PRIVATE include third_party)
endif()

# ====================================================================
# 4. 輸出路徑與各平台客製化動態庫處理
# ====================================================================
if(WIN32)
    # 1. 設定 Windows 的輸出目錄
    set(WIN_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build_win/ytoncmd-win-${VERSION_STR}")
    
    set_target_properties(ytoncmd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${WIN_OUTPUT_DIR}"
    )
    
    # 2. 自動尋找並複製 FFmpeg 的 DLL 檔案
    set(FFMPEG_BIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-8.1.1-full_build-shared/bin")
    
    if(EXISTS "${FFMPEG_BIN_DIR}")
        message(STATUS "FFmpeg bin directory found: ${FFMPEG_BIN_DIR}")
        add_custom_command(TARGET ytoncmd POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WIN_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${FFMPEG_BIN_DIR}" "${WIN_OUTPUT_DIR}"
            COMMENT "Copying Windows FFmpeg DLLs and executables to output directory..."
        )
    else()
        message(WARNING "FFmpeg bin directory NOT found at: ${FFMPEG_BIN_DIR}. DLLs will not be copied automatically.")
    endif()

else()
    # 類 Unix 系統 (Linux / macOS)：根據作業系統動態決定資料夾名稱
    if(APPLE)
        set(PLATFORM_NAME "macos")
    else()
        set(PLATFORM_NAME "linux")
    endif()

    # 1. 設定動態的輸出目錄 (如 build/ytoncmd-linux-v1.1 或 build/ytoncmd-macos-v1.1)
    set(UNIX_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/ytoncmd-${PLATFORM_NAME}-${VERSION_STR}")
    
    set_target_properties(ytoncmd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${UNIX_OUTPUT_DIR}"
    )

    # 2. 針對 Linux 進行特殊的 rpath 處理（Mac 不需要 $ORIGIN，Mac 本身由 Homebrew 處理好相容路義）
    if(NOT APPLE)
        set_target_properties(ytoncmd PROPERTIES
            BUILD_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "$ORIGIN"
        )
        
        # 如果未來你有 Linux 的 .so 檔案需要自動複製
        set(LINUX_SO_DIR "${CMAKE_SOURCE_DIR}/path_to_your_linux_so_folder")
        if(EXISTS "${LINUX_SO_DIR}")
            add_custom_command(TARGET ytoncmd POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${LINUX_SO_DIR}" "${UNIX_OUTPUT_DIR}"
                COMMENT "Copying Linux .so libraries to output directory..."
            )
        endif()
    endif()
endif()