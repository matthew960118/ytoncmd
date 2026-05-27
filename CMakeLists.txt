cmake_minimum_required(VERSION 3.10)
project(ytoncmd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include third_party)
set(VERSION_STR "v1.1")

# 【修改這段】：如果是編譯給 Windows，就跳過 PkgConfig
if(WIN32)
    message(STATUS "Detected Windows target, skipping Linux pkg-config.")
    # 這裡什麼都不用做，因為我們把路徑寫在工具鏈檔案裡了
else()
    # 只有原生在 Linux 編譯時才用 PkgConfig
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale libswresample)
endif()

file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp" "main.cpp")

add_executable(ytoncmd ${SOURCES})

# 連結 FFmpeg 庫
target_link_libraries(ytoncmd PRIVATE ${FFMPEG_LIBRARIES})

if(WIN32)
    # 1. 設定 Windows 的輸出目錄為專案根目錄下的 build_win/ytoncmd-win-v1.0/
    set(WIN_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build_win/ytoncmd-win-${VERSION_STR}")
    
    set_target_properties(ytoncmd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${WIN_OUTPUT_DIR}"
    )
    
    # 2. 自動尋找並複製 FFmpeg 的 DLL 檔案
    # 依據你的目錄結構，FFmpeg bin 目錄在專案根目錄下的 ffmpeg-8.1.1-full_build-shared/bin
    set(FFMPEG_BIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-8.1.1-full_build-shared/bin")
    
    if(EXISTS "${FFMPEG_BIN_DIR}")
        message(STATUS "FFmpeg bin directory found: ${FFMPEG_BIN_DIR}")
        
        # 在編譯完成後執行複製動作
        add_custom_command(TARGET ytoncmd POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WIN_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${FFMPEG_BIN_DIR}" "${WIN_OUTPUT_DIR}"
            COMMENT "Copying Windows FFmpeg DLLs and executables to output directory..."
        )
    else()
        message(WARNING "FFmpeg bin directory NOT found at: ${FFMPEG_BIN_DIR}. DLLs will not be copied automatically.")
    endif()

else()
    # 1. 設定 Linux 的輸出目錄為專案根目錄下的 build/ytoncmd-linux-v1.0/
    set(LINUX_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/ytoncmd-linux-${VERSION_STR}")
    
    set_target_properties(ytoncmd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${LINUX_OUTPUT_DIR}"
        
        # ----------- 新增這兩行 Linux 專屬的 rpath 設定 -----------
        # 允許在主程式身邊尋找 .so 檔案 ($ORIGIN 在 Linux 代表執行檔所在的當前目錄)
        BUILD_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN"
        # --------------------------------------------------------
    )

    # 2. 【選填】如果未來你有 Linux 的 .so 檔案需要像 Windows 那樣自動複製
    # 假設你把 Linux 的 .so 放在專案的某個資料夾，例如第三方庫
    set(LINUX_SO_DIR "${CMAKE_SOURCE_DIR}/path_to_your_linux_so_folder")
    
    if(EXISTS "${LINUX_SO_DIR}")
        add_custom_command(TARGET ytoncmd POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${LINUX_SO_DIR}" "${LINUX_OUTPUT_DIR}"
            COMMENT "Copying Linux .so libraries to output directory..."
        )
    endif()
endif()