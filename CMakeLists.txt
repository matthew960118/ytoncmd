cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(ytoncmd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(VERSION_STR "v1.3")

# 定義通用原始碼
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp" "main.cpp")
add_executable(ytoncmd ${SOURCES})

# ====================================================================
# 方案 A：Windows 平台處理（完成後直接 return 結束，避免後續縮排）
# ====================================================================
if(WIN32)
    option(YTONCMD_WINDOWS_STATIC_FFMPEG "Link Windows/MSVC against a static FFmpeg package" OFF)
    set(YTONCMD_WINDOWS_STATIC_FFMPEG_DIR "" CACHE PATH "Path to a Windows/MSVC static FFmpeg SDK")

    target_compile_definitions(ytoncmd PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)

    # 區分 MSVC 與 MinGW 設置路徑
    if(MSVC)
        set_property(TARGET ytoncmd PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_target_properties(ytoncmd PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

        if(YTONCMD_WINDOWS_STATIC_FFMPEG)
            message(STATUS "Detected Windows Native MSVC environment. Setting up STATIC FFmpeg paths.")
            set(FFMPEG_WIN_LOCAL_STATIC_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-8.1.1-msvc-x64-static-mt")
            if(YTONCMD_WINDOWS_STATIC_FFMPEG_DIR)
                set(FFMPEG_WIN_DIR "${YTONCMD_WINDOWS_STATIC_FFMPEG_DIR}")
            elseif(EXISTS "${FFMPEG_WIN_LOCAL_STATIC_DIR}/include" AND EXISTS "${FFMPEG_WIN_LOCAL_STATIC_DIR}/lib")
                set(FFMPEG_WIN_DIR "${FFMPEG_WIN_LOCAL_STATIC_DIR}")
            else()
                set(FFMPEG_WIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-static")
            endif()
        else()
            message(STATUS "Detected Windows Native MSVC environment. Setting up SHARED FFmpeg paths.")
            set(FFMPEG_WIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-shared")
        endif()
    else()
        message(STATUS "Detected Windows Cross-Compilation (MinGW). Setting up SHARED FFmpeg paths.")
        if(NOT FFMPEG_WIN_DIR)
            set(FFMPEG_WIN_DIR "${CMAKE_SOURCE_DIR}/ffmpeg-8.1.1-full_build-shared" CACHE PATH "Path to a Windows/MinGW shared FFmpeg SDK")
        endif()
    endif()

    message(STATUS "Using Windows FFmpeg directory: ${FFMPEG_WIN_DIR}")
    if(NOT EXISTS "${FFMPEG_WIN_DIR}/include" OR NOT EXISTS "${FFMPEG_WIN_DIR}/lib")
        message(FATAL_ERROR "Windows FFmpeg package not found. Expected include/ and lib/ under: ${FFMPEG_WIN_DIR}")
    endif()

    # 處理 Windows 庫連結
    if(MSVC)
        if(YTONCMD_WINDOWS_STATIC_FFMPEG)
            set(FFMPEG_WIN_CORE_BASENAMES avformat avcodec swscale swresample avutil)
            foreach(FFMPEG_WIN_CORE_BASENAME IN LISTS FFMPEG_WIN_CORE_BASENAMES)
                set(FFMPEG_WIN_CORE_LIB_A "${FFMPEG_WIN_DIR}/lib/lib${FFMPEG_WIN_CORE_BASENAME}.a")
                set(FFMPEG_WIN_CORE_LIB_LIB "${FFMPEG_WIN_DIR}/lib/${FFMPEG_WIN_CORE_BASENAME}.lib")
                if(EXISTS "${FFMPEG_WIN_CORE_LIB_A}")
                    list(APPEND FFMPEG_WIN_LIBS "${FFMPEG_WIN_CORE_LIB_A}")
                elseif(EXISTS "${FFMPEG_WIN_CORE_LIB_LIB}")
                    list(APPEND FFMPEG_WIN_LIBS "${FFMPEG_WIN_CORE_LIB_LIB}")
                else()
                    message(FATAL_ERROR "Required Windows static FFmpeg core library not found for: ${FFMPEG_WIN_CORE_BASENAME}")
                endif()
            endforeach()

            set(FFMPEG_WIN_CORE_LIBS ${FFMPEG_WIN_LIBS})
            file(GLOB FFMPEG_WIN_DEP_LIBS CONFIGURE_DEPENDS "${FFMPEG_WIN_DIR}/lib/*.lib")
            foreach(FFMPEG_WIN_DEP_LIB IN LISTS FFMPEG_WIN_DEP_LIBS)
                if(NOT FFMPEG_WIN_DEP_LIB IN_LIST FFMPEG_WIN_CORE_LIBS)
                    list(APPEND FFMPEG_WIN_LIBS "${FFMPEG_WIN_DEP_LIB}")
                endif()
            endforeach()
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

        set(FFMPEG_WIN_SYSTEM_LIBS ws2_32 Bcrypt Secur32 Ncrypt Strmiids Vfw32 Shlwapi Mfuuid d3d11 dxva2 ole32 user32 advapi32 gdi32 shell32 Crypt32 Normaliz Version Userenv)
        target_link_libraries(ytoncmd PRIVATE ${FFMPEG_WIN_LIBS} ${FFMPEG_WIN_SYSTEM_LIBS})
    else()
        # MinGW 連結
        set(FFMPEG_LIBRARIES avformat avcodec avutil swscale swresample)
        link_directories("${FFMPEG_WIN_DIR}/lib")
        set(FFMPEG_WIN_SYSTEM_LIBS ws2_32 bcrypt secur32 ncrypt strmiids vfw32 shlwapi mfuuid d3d11 dxva2 ole32 user32 advapi32 gdi32 shell32 crypt32 normaliz version userenv)
        target_link_libraries(ytoncmd PRIVATE ${FFMPEG_LIBRARIES} ${FFMPEG_WIN_SYSTEM_LIBS})
    endif()

    target_include_directories(ytoncmd PRIVATE include third_party "${FFMPEG_WIN_DIR}/include")

    # Windows 輸出與 DLL 複製
    set(WIN_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build_win/ytoncmd-win-${VERSION_STR}")
    set_target_properties(ytoncmd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${WIN_OUTPUT_DIR}")

    if(NOT YTONCMD_WINDOWS_STATIC_FFMPEG)
        set(FFMPEG_WIN_BIN_DIR "${FFMPEG_WIN_DIR}/bin")
        if(NOT EXISTS "${FFMPEG_WIN_BIN_DIR}")
            message(FATAL_ERROR "Windows shared FFmpeg bin directory not found: ${FFMPEG_WIN_BIN_DIR}")
        endif()
        set(REQUIRED_FFMPEG_DLL_NAMES "avcodec-62.dll" "avformat-62.dll" "avutil-60.dll" "swresample-6.dll" "swscale-9.dll")
        set(FFMPEG_WIN_DLLS "")
        foreach(DLL_NAME ${REQUIRED_FFMPEG_DLL_NAMES})
            set(FULL_DLL_PATH "${FFMPEG_WIN_BIN_DIR}/${DLL_NAME}")
            if(EXISTS "${FULL_DLL_PATH}")
                list(APPEND FFMPEG_WIN_DLLS "${FULL_DLL_PATH}")
            else()
                message(FATAL_ERROR "Required FFmpeg DLL not found: ${FULL_DLL_PATH}")
            endif()
        endforeach()
        add_custom_command(TARGET ytoncmd POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ytoncmd>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FFMPEG_WIN_DLLS} "$<TARGET_FILE_DIR:ytoncmd>"
            COMMENT "Copying Windows FFmpeg shared DLLs to output directory..."
        )
    endif()

    return() # 結束 Windows 處理，後面全是 Unix 系統的世界
endif()

# ====================================================================
# 5. Unix-like 共同依賴偵測（Linux & macOS）
# ====================================================================
find_package(PkgConfig REQUIRED)

# ====================================================================
# 方案 B：macOS 平台處理
# ====================================================================
# if(APPLE)
#     message(STATUS "Detected macOS target. Enabling STATIC link for FFmpeg.")
#     set(PKG_CONFIG_ARGN "--static")
#     pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)

#     target_link_libraries(ytoncmd PRIVATE PkgConfig::FFMPEG)
#     target_include_directories(ytoncmd PRIVATE include third_party)

#     set(UNIX_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/ytoncmd-macos-${VERSION_STR}")
#     set_target_properties(ytoncmd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${UNIX_OUTPUT_DIR}")
#     return()
# endif()

# ====================================================================
# 方案 C：Linux 平台處理（Fedora / Ubuntu 等）
# ====================================================================
message(STATUS "Detected Linux target. Enabling DYNAMIC link for FFmpeg.")
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)

target_link_libraries(ytoncmd PRIVATE PkgConfig::FFMPEG)

# 顯式將 PkgConfig 掃描到的 `FFMPEG_INCLUDE_DIRS` 餵進來（包含 Fedora 的 /usr/include/ffmpeg）
target_include_directories(ytoncmd PRIVATE 
    include 
    third_party 
    ${FFMPEG_INCLUDE_DIRS}
)

set(UNIX_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/ytoncmd-linux-${VERSION_STR}")
set_target_properties(ytoncmd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${UNIX_OUTPUT_DIR}")

set_target_properties(ytoncmd PROPERTIES
    BUILD_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
)