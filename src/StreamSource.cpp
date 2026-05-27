#include "StreamSource.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <filesystem>
extern "C" {
#include <libavutil/mem.h>
}

#ifdef _WIN32
static const char* kWindowsCacheFile = "win_stream_cache.mkv";
const std::string kLocalYtDlp = "yt-dlp.exe";
#endif

StreamSource::StreamSource(const std::string &path, int tw, int th, double r)
    : MediaSource(path, tw, th, r, true)
{
    if (open(path))
    {
        if (!initializeDecoder())
            std::cerr << "[StreamSource] 錯誤: 初始化解碼器失敗。\n";
    }
    else
    {
        std::cerr << "[StreamSource] 錯誤: 無法綁定資料管道。\n";
    }
}

StreamSource::~StreamSource()
{
    closeCurrentConnection();
    closeAvio();
    closePipe();
}

void StreamSource::closeAvio()
{
    if (avio_ctx)
    {
        av_freep(&avio_ctx->buffer);
        avio_context_free(&avio_ctx);
        avio_buffer = nullptr;
    }
    else if (avio_buffer)
    {
        av_freep(&avio_buffer);
    }
}

void StreamSource::closePipe()
{
    if (!m_pipe_handle)
        return;

#ifdef _WIN32
    std::fclose(m_pipe_handle);
    std::remove(kWindowsCacheFile);
#else
    pclose(m_pipe_handle);
#endif
    m_pipe_handle = nullptr;
}

bool ensureYtDlp()
{
#ifdef _WIN32
    // 1. 優先檢查：當前專案目錄下有沒有 yt-dlp.exe
    if (std::filesystem::exists(kLocalYtDlp))
    {
        // 同目錄有找到，直接使用
        return true;
    }

    // 2. 次要檢查：系統環境變數 (PATH) 裡面有沒有 yt-dlp
    // where 指令如果回傳 0 代表系統全域有裝
    if (std::system("where yt-dlp >nul 2>nul") == 0)
    {
        return true;
    }

    // 3. 終極觸發：前兩者都沒有，開始自動下載到同目錄
    std::cout << "[資訊] 偵測到系統與專案目錄皆無 yt-dlp，準備自動下載最新版本...\n";
    std::cout << "正在從 GitHub 下載 yt-dlp.exe，請稍候...\n";

    // 呼叫 Windows 內建的 PowerShell 進行下載
    std::string downloadCmd = "powershell -Command \"Invoke-WebRequest -Uri 'https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe' -OutFile '" + kLocalYtDlp + "'\"";
    
    int result = std::system(downloadCmd.c_str());
    
    if (result == 0 && std::filesystem::exists(kLocalYtDlp))
    {
        std::cout << "[成功] yt-dlp.exe 下載完成並已存至專案目錄！\n";
        return true;
    }
    else
    {
        std::cerr << "[錯誤] 自動下載失敗。請嘗試手動下載並放入專案目錄：\n"
                  << "👉 https://github.com/yt-dlp/yt-dlp/releases\n";
        return false;
    }
#else
    // Linux 保持原樣：只檢查系統
    if (std::system("which yt-dlp >/dev/null 2>&1") != 0)
    {
        std::cerr << "\n====== 偵測到系統缺少必要元件 ======\n"
                  << "   [提示] 找不到 yt-dlp 核心\n"
                  << "---------------------------------------\n"
                  << "請在終端機執行以下指令一鍵安裝：\n\n"
                  << "   sudo wget https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -O /usr/local/bin/yt-dlp && sudo chmod a+rx /usr/local/bin/yt-dlp\n\n"
                  << "=======================================\n\n";
        return false;
    }
    return true;
#endif
}

bool StreamSource::openPipe()
{
    if (!ensureYtDlp())
    {
        return false;
    }
    closePipe();

#ifdef _WIN32
    std::remove(kWindowsCacheFile);
    std::string ytDlpCmd = std::filesystem::exists(kLocalYtDlp) ? ".\\" + kLocalYtDlp : "yt-dlp";
    // std::string cmd = "start /B " + ytDlpCmd +" -q --no-playlist -f \"b\" --merge-output-format mkv \"" + m_path + "\" -o " + kWindowsCacheFile;
    std::string cmd = "start /B " + ytDlpCmd +" -q --no-playlist --merge-output-format mkv \"" + m_path + "\" -o " + kWindowsCacheFile;
    std::system(cmd.c_str());

    for (int attempt = 0; attempt < 50; ++attempt)
    {
        m_pipe_handle = std::fopen(kWindowsCacheFile, "rb");
        if (m_pipe_handle)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
#else
    // std::string cmd = "yt-dlp -q --no-playlist -f \"b\" --merge-output-format mkv -o - \"" + m_path + "\"";
    std::string cmd = "yt-dlp -q --no-playlist --merge-output-format mkv -o - \"" + m_path + "\"";
    m_pipe_handle = popen(cmd.c_str(), "r");
    return m_pipe_handle != nullptr;
#endif
}

int StreamSource::readCallback(void* opaque, uint8_t* buf, int buf_size)
{
    StreamSource* h = static_cast<StreamSource*>(opaque);
    if (!h->m_pipe_handle)
        return AVERROR_EOF;

    size_t bytes_read = fread(buf, 1, buf_size, h->m_pipe_handle);
    if (bytes_read == 0)
        return AVERROR_EOF;

    return static_cast<int>(bytes_read);
}

bool StreamSource::open(const std::string& path)
{
    m_path = path;
    closeAvio();

    if (!openPipe())
        return false;

    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx)
        return false;

    avio_buffer = static_cast<uint8_t*>(av_malloc(kAvioBufferSize));
    if (!avio_buffer)
        return false;

    avio_ctx = avio_alloc_context(
        avio_buffer, kAvioBufferSize,
        0,
        this,
        &StreamSource::readCallback,
        NULL,
        NULL);
    if (!avio_ctx)
        return false;

    fmt_ctx->pb = avio_ctx;
    fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

    if (avformat_open_input(&fmt_ctx, NULL, NULL, NULL) < 0)
        return false;
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
        return false;

    return true;
}

