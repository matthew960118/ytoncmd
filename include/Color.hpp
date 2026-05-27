#ifndef COLOR_HPP
#define COLOR_HPP
#include <cstdint>

struct Color {
    uint8_t r, g, b;

    // 手動加上這個建構子
    // 使用初始化列表 (Initializer List) 效能最好
    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) 
        : r(r), g(g), b(b) {}
};

#endif