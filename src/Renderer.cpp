#include "Renderer.hpp"
#include "MediaSource.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>
#include <csignal>

#ifdef _WIN32
#include <conio.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

extern volatile std::sig_atomic_t g_interrupted;

namespace
{
#ifndef _WIN32
    class TerminalRawMode
    {
    public:
        TerminalRawMode()
        {
            active = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &old_term) == 0;
            if (active)
            {
                termios raw = old_term;
                raw.c_lflag &= static_cast<unsigned int>(~(ICANON | ECHO));
                tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            }
        }

        ~TerminalRawMode()
        {
            if (active)
                tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
        }

    private:
        termios old_term{};
        bool active = false;
    };
#endif

    bool userRequestedQuit()
    {
        if (g_interrupted != 0)
            return true;

#ifdef _WIN32
        if (!_kbhit())
            return false;
        int ch = _getch();
        return ch == 'q' || ch == 'Q';
#else
        timeval tv{0, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0)
            return false;

        char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) != 1)
            return false;
        return ch == 'q' || ch == 'Q';
#endif
    }

}

Renderer::Renderer(int w, int h) : width(w), height(h)
{
    last_frame.assign(w * h, {0, 0, 0});
    buffer.reserve(w * h * 32);
}

static inline void appendNumToBuf(std::string &buf, int val)
{
    if (val == 0)
    {
        buf.push_back('0');
        return;
    }

    char local_buf[12];
    int i = 12;

    // 從個位數開始往前逆向填入數值
    while (val > 0)
    {
        local_buf[--i] = static_cast<char>('0' + (val % 10));
        val /= 10;
    }

    // 一次性把整段字串加進 buffer
    buf.append(local_buf + i, 12 - i);
}

void Renderer::render(const std::vector<Color> &current_frame)
{
    if (current_frame.size() < last_frame.size())
        return;

    buffer.clear();
    buffer += "\033[?2026h";

    // ➔ 修正：使用 (height + 1) / 2 進行無條件進位，確保奇數行時最後一行也能跑到
    int terminal_height = (height + 1) / 2;

    for (int y = 0; y < terminal_height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int r_y_up = y * 2;
            int r_y_down = y * 2 + 1;

            int idx_up = r_y_up * width + x;
            const Color &c2 = current_frame[idx_up]; // 上半部永遠存在
            const Color &l2 = last_frame[idx_up];

            // 判斷下半部是否超界
            bool has_down_pixel = (r_y_down < height);

            // 如果下半部存在，正常取值；如果超界（奇數最後一行），下半部當作純黑 {0,0,0}
            int idx_down = has_down_pixel ? (r_y_down * width + x) : -1;
            Color c1 = has_down_pixel ? current_frame[idx_down] : Color{0, 0, 0};
            Color l1 = has_down_pixel ? last_frame[idx_down] : Color{0, 0, 0};

            // 只要上下任一像素有變動，這一個字元格子就需要重畫
            if (first_frame ||
                c1.r != l1.r || c1.g != l1.g || c1.b != l1.b ||
                c2.r != l2.r || c2.g != l2.g || c2.b != l2.b)
            {
                int terminal_y = y + 1;

                // 檢查游標定位
                if (terminal_y != last_terminal_y || x != last_terminal_x)
                {
                    buffer += "\033[";
                    appendNumToBuf(buffer, terminal_y);
                    buffer.push_back(';');
                    appendNumToBuf(buffer, x + 1);
                    buffer.push_back('H');
                }

                // 處理下半部像素 c1 背景色 (48)
                if (has_down_pixel)
                {
                    if (!has_terminal_color ||
                        c1.r != last_terminal_color_down.r ||
                        c1.g != last_terminal_color_down.g ||
                        c1.b != last_terminal_color_down.b)
                    {
                        buffer += "\033[48;2;";
                        appendNumToBuf(buffer, c1.r);
                        buffer.push_back(';');
                        appendNumToBuf(buffer, c1.g);
                        buffer.push_back(';');
                        appendNumToBuf(buffer, c1.b);
                        buffer.push_back('m');

                        last_terminal_color_down = c1;
                    }
                }
                else
                {
                    // 奇數邊界處理：如果沒有下半部像素，將終端機背景色重設為預設（通常是黑色）
                    // 避免受到上一次殘留背景色的污染
                    buffer += "\033[49m";
                    last_terminal_color_down = {0, 0, 0}; // 重設快取
                }

                // 處理上半部像素 c2 前景色 (38)
                if (!has_terminal_color ||
                    c2.r != last_terminal_color_up.r ||
                    c2.g != last_terminal_color_up.g ||
                    c2.b != last_terminal_color_up.b)
                {
                    buffer += "\033[38;2;";
                    appendNumToBuf(buffer, c2.r);
                    buffer.push_back(';');
                    appendNumToBuf(buffer, c2.g);
                    buffer.push_back(';');
                    appendNumToBuf(buffer, c2.b);
                    buffer.push_back('m');

                    last_terminal_color_up = c2;
                }

                has_terminal_color = true;

                // 輸出半塊字元
                buffer += "▀";

                // 游標邊界預測更新
                if (x == width - 1)
                {
                    last_terminal_y = terminal_y + 1;
                    last_terminal_x = 0;
                }
                else
                {
                    last_terminal_y = terminal_y;
                    last_terminal_x = x + 1;
                }
            }
        }
    }

    buffer += "\033[?2026l";

    if (buffer.size() > 16)
    {
        fwrite(buffer.data(), 1, buffer.size(), stdout);
        fflush(stdout);
    }

    last_frame = current_frame;
    first_frame = false;
}
void Renderer::save(const char *path, const std::vector<Color> &current_frame)
{
    if (current_frame.size() < last_frame.size())
        return;

    buffer.clear();
    FILE *file = fopen(path, "wb");
    if (file == NULL)
    {
        perror("無法創建或打開檔案");
        return;
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int idx = y * width + x;
            const Color &c = current_frame[idx];
            buffer += "\033[48;2;" + std::to_string(c.r) + ";" +
                      std::to_string(c.g) + ";" + std::to_string(c.b) + "m ";
        }
        buffer += "\n";
    }
    buffer += "\033[0m";
    fwrite(buffer.c_str(), 1, buffer.size(), file);
    fclose(file);
}

void Renderer::save(const char *path, const std::vector<Color> &current_frame)
{
    if (current_frame.size() < last_frame.size())
        return;

    FILE *file = fopen(path, "wb");
    if (file == NULL)
    {
        perror("無法創建或打開檔案");
        return;
    }

    buffer.clear();
    buffer += "\033[?2026h";

    // ➔ 修正：使用 (height + 1) / 2 進行無條件進位，確保奇數行時最後一行也能跑到
    int terminal_height = (height + 1) / 2;

    for (int y = 0; y < terminal_height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int r_y_up = y * 2;
            int r_y_down = y * 2 + 1;

            int idx_up = r_y_up * width + x;
            const Color &c2 = current_frame[idx_up]; // 上半部永遠存在
            const Color &l2 = last_frame[idx_up];

            // 判斷下半部是否超界
            bool has_down_pixel = (r_y_down < height);

            // 如果下半部存在，正常取值；如果超界（奇數最後一行），下半部當作純黑 {0,0,0}
            int idx_down = has_down_pixel ? (r_y_down * width + x) : -1;
            Color c1 = has_down_pixel ? current_frame[idx_down] : Color{0, 0, 0};
            Color l1 = has_down_pixel ? last_frame[idx_down] : Color{0, 0, 0};

            // 只要上下任一像素有變動，這一個字元格子就需要重畫
            if (first_frame ||
                c1.r != l1.r || c1.g != l1.g || c1.b != l1.b ||
                c2.r != l2.r || c2.g != l2.g || c2.b != l2.b)
            {
                int terminal_y = y + 1;

                // 檢查游標定位
                if (terminal_y != last_terminal_y || x != last_terminal_x)
                {
                    buffer += "\033[";
                    appendNumToBuf(buffer, terminal_y);
                    buffer.push_back(';');
                    appendNumToBuf(buffer, x + 1);
                    buffer.push_back('H');
                }

                // 處理下半部像素 c1 背景色 (48)
                if (has_down_pixel)
                {
                    if (!has_terminal_color ||
                        c1.r != last_terminal_color_down.r ||
                        c1.g != last_terminal_color_down.g ||
                        c1.b != last_terminal_color_down.b)
                    {
                        buffer += "\033[48;2;";
                        appendNumToBuf(buffer, c1.r);
                        buffer.push_back(';');
                        appendNumToBuf(buffer, c1.g);
                        buffer.push_back(';');
                        appendNumToBuf(buffer, c1.b);
                        buffer.push_back('m');

                        last_terminal_color_down = c1;
                    }
                }
                else
                {
                    // 奇數邊界處理：如果沒有下半部像素，將終端機背景色重設為預設（通常是黑色）
                    // 避免受到上一次殘留背景色的污染
                    buffer += "\033[49m";
                    last_terminal_color_down = {0, 0, 0}; // 重設快取
                }

                // 處理上半部像素 c2 前景色 (38)
                if (!has_terminal_color ||
                    c2.r != last_terminal_color_up.r ||
                    c2.g != last_terminal_color_up.g ||
                    c2.b != last_terminal_color_up.b)
                {
                    buffer += "\033[38;2;";
                    appendNumToBuf(buffer, c2.r);
                    buffer.push_back(';');
                    appendNumToBuf(buffer, c2.g);
                    buffer.push_back(';');
                    appendNumToBuf(buffer, c2.b);
                    buffer.push_back('m');

                    last_terminal_color_up = c2;
                }

                has_terminal_color = true;

                // 輸出半塊字元
                buffer += "▀";

                // 游標邊界預測更新
                if (x == width - 1)
                {
                    last_terminal_y = terminal_y + 1;
                    last_terminal_x = 0;
                }
                else
                {
                    last_terminal_y = terminal_y;
                    last_terminal_x = x + 1;
                }
            }
        }
    }

    buffer += "\033[?2026l";
    fwrite(buffer.c_str(), 1, buffer.size(), file);
    fclose(file);
}

void Renderer::play(MediaSource &src)
{
    std::vector<Color> canvas(width * height, {0, 0, 0});
    double fps = src.getFPS() > 0.0 ? src.getFPS() : 30.0;
    auto frame_duration = std::chrono::duration<double, std::milli>(1000.0 / fps);
    auto next_frame_time = std::chrono::steady_clock::now();

#ifndef _WIN32
    TerminalRawMode raw_mode;
#endif
    first_frame = true;
    std::cout << "\033[?25l";

    while (src.getNextFrame(canvas))
    {
        if (userRequestedQuit())
            break;

        render(canvas);

        if (!src.hasAudio())
        {
            next_frame_time += std::chrono::duration_cast<std::chrono::steady_clock::duration>(frame_duration);
            std::this_thread::sleep_until(next_frame_time);
            if (std::chrono::steady_clock::now() > next_frame_time + std::chrono::milliseconds(250))
                next_frame_time = std::chrono::steady_clock::now();
        }
        else
        {
            next_frame_time = std::chrono::steady_clock::now();
        }
    }

    std::cout << "\033[0m\033[?25h" << std::endl;
}
