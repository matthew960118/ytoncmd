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
    Color last_terminal_color_up, last_terminal_color_down;
    bool has_terminal_color = false;
    int last_terminal_y = 0, last_terminal_x = 0;
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