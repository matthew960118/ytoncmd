#ifndef STREAMSOURCE_HPP
#define STREAMSOURCE_HPP

#include "MediaSource.hpp"
extern "C" {
#include <libavformat/avio.h>
}
#include <cstdio>
#include <filesystem>

class StreamSource : public MediaSource {
private:
    FILE* m_pipe_handle = nullptr;
    AVIOContext* avio_ctx = nullptr;
    uint8_t* avio_buffer = nullptr;
    std::filesystem::path yt_dlp_path;
    static constexpr int kAvioBufferSize = 16384;

    static int readCallback(void* opaque, uint8_t* buf, int buf_size);
    std::filesystem::path getExecutableDir() const;
    std::filesystem::path getYtDlpFilename() const;
    std::string quoteForShell(const std::filesystem::path& path) const;
    bool ensureYtDlpBinary();
    bool downloadYtDlpBinary(const std::filesystem::path& dest) const;
    bool openPipe();
    void closePipe();
    void closeAvio();

public:
    StreamSource(const std::string &path, int tw, int th, double r);
    ~StreamSource() override;

    bool open(const std::string& path) override;
};

#endif
