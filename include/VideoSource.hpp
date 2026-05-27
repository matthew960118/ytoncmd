#ifndef VIDEOSOURCE_HPP
#define VIDEOSOURCE_HPP

#include "MediaSource.hpp"

class VideoSource : public MediaSource {
public:
    VideoSource(const std::string &filename, int tw, int th, double r);
    ~VideoSource() override; // 🌟 宣告普通解構子，實作寫在 .cpp 裡

    bool open(const std::string& path) override;
};

#endif