#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <string>
#include <vector>
#include <string>
#include <cstdint>
#include "Color.hpp"
// class VideoSource;
class MediaSource;

class Renderer {
private:
    int width, height;
    std::vector<Color> last_frame;
    std::string buffer;
    bool first_frame = true;

public:
    Renderer(int w, int h);
    void render(const std::vector<Color>& current_frame);
    void save(const char *path, const std::vector<Color>& current_frame);
    // void play(VideoSource& src);
    void play(MediaSource& src);

};

#endif