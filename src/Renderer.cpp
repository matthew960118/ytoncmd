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

namespace {
#ifndef _WIN32
class TerminalRawMode {
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

void Renderer::render(const std::vector<Color> &current_frame)
{
    if (current_frame.size() < last_frame.size())
        return;

    buffer.clear();
    buffer += "\033[H";

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int idx = y * width + x;
            const Color &c = current_frame[idx];
            const Color &l = last_frame[idx];

            if (first_frame || c.r != l.r || c.g != l.g || c.b != l.b)
            {
                buffer += "\033[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
                buffer += "\033[48;2;" + std::to_string(c.r) + ";" +
                          std::to_string(c.g) + ";" + std::to_string(c.b) + "m ";
            }
        }
    }
    fwrite(buffer.c_str(), 1, buffer.size(), stdout);
    fflush(stdout);
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
