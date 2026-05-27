#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include "ImageSource.hpp"
#include <algorithm>
#include <iostream>

ImageSource::ImageSource(const std::string &img_path, int tw, int th)
    : path(img_path), target_w(tw), target_h(th)
{
    data = stbi_load(path.c_str(), &width, &height, &channels, 3);
    loaded = data != nullptr && width > 0 && height > 0 && target_w > 0 && target_h > 0;

    if (!loaded)
        std::cerr << "錯誤: 無法讀取圖片或圖片尺寸不合法: " << path << std::endl;
}

ImageSource::~ImageSource()
{
    if (data)
        stbi_image_free(data);
}

Color ImageSource::getColor(int row, int col) const
{
    int idx = (row * width + col) * 3;
    return Color{data[idx], data[idx + 1], data[idx + 2]};
}

std::vector<Color> ImageSource::getPixels() const
{
    std::vector<Color> pixels(target_w * target_h, {0, 0, 0});
    if (!loaded)
        return pixels;

    float fontW = 9.0f, fontH = 19.0f;
    float scaleX = static_cast<float>(target_w) / width;
    float scaleY = static_cast<float>(target_h) / (height * (fontW / fontH));
    float scale = std::min(scaleX, scaleY);

    int newW = std::max(1, static_cast<int>(width * scale));
    int newH = std::max(1, static_cast<int>(height * scale * (fontW / fontH)));
    newW = std::min(newW, target_w);
    newH = std::min(newH, target_h);

    std::vector<uint8_t> resizedData(newW * newH * 3);
    stbir_resize_uint8_linear(
        data, width, height, 0,
        resizedData.data(), newW, newH, 0,
        STBIR_RGB);

    int offsetX = (target_w - newW) / 2;
    int offsetY = (target_h - newH) / 2;

    for (int y = 0; y < newH; ++y)
    {
        for (int x = 0; x < newW; ++x)
        {
            int srcIdx = (y * newW + x) * 3;
            int tarIdx = (y + offsetY) * target_w + (x + offsetX);
            pixels[tarIdx] = {resizedData[srcIdx], resizedData[srcIdx + 1], resizedData[srcIdx + 2]};
        }
    }

    return pixels;
}
