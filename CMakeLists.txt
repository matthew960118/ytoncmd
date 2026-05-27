cmake_minimum_required(VERSION 3.10)
project(ytoncmd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_STR "v1.1")

# ====================================================================
# 1. FFmpeg 依賴偵測區（區分 Windows 與 類Unix 系統）
# ====================================================================
if(WIN32)
    # 檢查是否為 Windows 原生 MSVC 編譯器（GitHub Actions 環境）
    if(MSVC)
        message(STATUS "Detected Windows Native MSVC environment. Setting up local FFmpeg paths.")
        
        # 定義 GitHub Actions 下載解壓後的 Windows FFmpeg 根目錄
        set(FFMPEG_WIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-8.1.1-full_build-shared")
        
        # 指定 Windows 尋找 .lib 檔的目錄
        link_directories("${FFMPEG_WIN_DIR}/lib")
        
        # 指定原生 Windows MSVC 點名要連結的標準 .lib 導入庫
        set(FFMPEG_WIN_LIBS avformat avcodec avutil swscale swresample)
    else()
        message(STATUS "Detected Windows Cross-Compilation (MinGW). Using environment variables.")
        # 如果是你原本在 Linux 上用工具鏈編譯 Windows，則維持你原本的傳統設定
        include_directories(include third_party)
    endif()
else()
    # 只有在 Linux 或 macOS (APPLE) 才啟用 PkgConfig
    find_package(PkgConfig REQUIRED)
    
    # 💡 加上 STATIC 關鍵字，強迫 Mac 機器人抓 .a 靜態庫，實現真．獨立單一執行檔
    pkg_check_modules(FFMPEG STATIC REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
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
        # 💡 原生 Windows MSVC：注入剛剛下載解壓縮的 FFmpeg 標頭檔路徑
        target_include_directories(ytoncmd PRIVATE 
            include 
            third_party
            "${FFMPEG_WIN_DIR}/include"
        )
        target_link_libraries(ytoncmd PRIVATE ${FFMPEG_WIN_LIBS})
    else()
        # 傳統 MinGW 交叉編譯：連結原本工具鏈指定的庫
        target_link_libraries(ytoncmd PRIVATE ${FFMPEG_LIBRARIES})
    endif()
else()
    # Linux & macOS 透過 PkgConfig 建立的虛擬 target 連結，它會自動把 .h 和 .so/.a 完美綁定
    # target_link_libraries(ytoncmd PRIVATE PkgConfig::FFMPEG)
    
    # # 確保本地的 include 與 third_party 也能被類 Unix 系統吃到
    # target_include_directories(ytoncmd PRIVATE include third_party)
    find_package(PkgConfig REQUIRED)
    if(APPLE)
        pkg_check_modules(FFMPEG STATIC REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
    else()
        pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
    endif()
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

    # 1. 設定動態的輸出目錄
    set(UNIX_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/ytoncmd-${PLATFORM_NAME}-${VERSION_STR}")
    
    set_target_properties(ytoncmd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${UNIX_OUTPUT_DIR}"
    )

    # 2. 針對 Linux 進行特殊的 rpath 處理
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