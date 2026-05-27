#include "VideoSource.hpp"
#include <iostream>

// 1. 實作建構子：必須主動把參數遞交給父類別 MediaSource
VideoSource::VideoSource(const std::string &filename, int tw, int th, double r)
    : MediaSource(filename, tw, th, r, false) // false 代表本地影片來源
{
    if (open(filename)) {
        if (!initializeDecoder()) {
            std::cerr << "[VideoSource] 錯誤: 初始化本地影片解碼器失敗。\n";
        }
    } else {
        std::cerr << "[VideoSource] 錯誤: 無法打開本地影片檔案: " << filename << "\n";
    }
}

// 2. 實作解構子：讓連結器找得到實體
VideoSource::~VideoSource() {
    // 這裡留空即可，父類別 ~MediaSource() 會自動被觸發去釋放 FFmpeg 資源
}

// 3. 實作打開檔案的專屬邏輯
bool VideoSource::open(const std::string& path) {
    if (avformat_open_input(&fmt_ctx, path.c_str(), NULL, NULL) < 0) 
        return false;
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) 
        return false;
    return true;
}