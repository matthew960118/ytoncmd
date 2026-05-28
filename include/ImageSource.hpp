#ifndef IMAGESOURCE_HPP
#define IMAGESOURCE_HPP

#include "Color.hpp"
#include <string>
#include <vector>

class ImageSource {
private:
    std::string path;
    int target_w, target_h;
    double ratio;
    unsigned char *data = nullptr;
    int width = 0, height = 0, channels = 0;
    bool loaded = false;

public:
    ImageSource(const std::string& img_path, int tw, int th, double r);
    ~ImageSource();

    bool isLoaded() const { return loaded; }
    Color getColor(int row, int col) const;
    std::vector<Color> getPixels() const;
};

#endif
