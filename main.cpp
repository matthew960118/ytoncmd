#include <iostream>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <csignal>
#include <cstring>
#include <exception>
#include "Renderer.hpp"
#include "ImageSource.hpp"
#include "VideoSource.hpp"
#include "StreamSource.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

struct TerminalSize
{
    int width;
    int height;
};

enum class FILETYPE
{
    IMAGE,
    VIDEO,
    URL,
    INVALID
};



FILETYPE testFileType(std::string &s)
{
    if (s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0)
        return FILETYPE::URL;

    size_t dotidx = s.rfind(".");
    if (dotidx == std::string::npos)
        return FILETYPE::INVALID;

    std::string ext = s.substr(dotidx);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".webp")
        return FILETYPE::IMAGE;
    if (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" || ext == ".webm")
        return FILETYPE::VIDEO;

    return FILETYPE::INVALID;
}

double parseCellRatio(const std::string& str) {
    // 預設值，以防解析完全失敗
    double default_ratio = 18.0 / 19.0; 

    // 尋找有沒有斜線 '/'
    size_t slash_pos = str.find('/');
    
    try {
        if (slash_pos != std::string::npos) {
            // 情況 A：格式是 "9/19"
            std::string num_str = str.substr(0, slash_pos);
            std::string den_str = str.substr(slash_pos + 1);
            
            double numerator = std::stod(num_str);   // 分子
            double denominator = std::stod(den_str); // 分母
            
            if (denominator == 0) {
                throw std::runtime_error("分母不能為 0");
            }
            return numerator / denominator;
        } else {
            // 情況 B：格式是普通的 "0.47"
            return std::stod(str);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[警告] 比例參數 \"" << str << "\" 格式錯誤，將沿用預設值 (9/19)。\n";
        return default_ratio;
    }
}

TerminalSize getTerminalSize()
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
    {
        int width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return {width, height};
    }
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1)
        return {w.ws_col, w.ws_row};
#endif
    return {80, 24};
}

void printHelp();

MediaSource* g_src_ptr = nullptr;
volatile std::sig_atomic_t g_interrupted = 0;

void signalHandler(int signum)
{
    (void)signum;
    g_interrupted = 1;

    const char reset[] = "\033[0m\033[?25h\n";
#ifdef _WIN32
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), reset, static_cast<DWORD>(std::strlen(reset)), &written, NULL);
#else
    write(STDOUT_FILENO, reset, sizeof(reset) - 1);
#endif
}

int main(int argc, char *argv[])
{
    std::signal(SIGINT, signalHandler);
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
    if (argc < 2)
    {
        printHelp();
        return 0;
    }

    std::string path = argv[1];
    if (path == "-h" || path == "--help")
    {
        printHelp();
        return 0;
    }

    FILETYPE ftype = testFileType(path);
    TerminalSize size;
    std::string outputPath;
    bool hasWidth = false;
    bool hasHeight = false;
    bool hasPath = false;
    float volume = 1.0f;
    double audioDelay = 0.0;
    double cell_ratio = 18.0 / 19.0;

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-o" || arg == "--output")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: Missing value for " << arg << std::endl;
                return 1;
            }
            outputPath = argv[++i];
            hasPath = true;
        }
        else if (arg == "-W" || arg == "--width")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: Missing value for " << arg << std::endl;
                return 1;
            }
            size.width = std::stoi(argv[++i]);
            hasWidth = true;
        }
        else if (arg == "-H" || arg == "--height")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: Missing value for " << arg << std::endl;
                return 1;
            }
            size.height = std::stoi(argv[++i]);
            hasHeight = true;
        }
        else if (arg == "-v" || arg == "--volume")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: Missing value for " << arg << std::endl;
                return 1;
            }
            try
            {
                volume = std::stof(argv[++i]);
            }
            catch (const std::exception&)
            {
                std::cerr << "Error: volume must be a number from 0.0 to 2.0" << std::endl;
                return 1;
            }
        }
        else if ((arg == "-r" || arg == "--cell-ratio") && i + 1 < argc) {
            std::string ratio_arg = argv[++i]; // 抓取下一個參數，例如 "9/19"
            cell_ratio = parseCellRatio(ratio_arg);
        }
        else if (arg == "--audio-delay")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: Missing value for " << arg << std::endl;
                return 1;
            }
            try
            {
                audioDelay = std::stod(argv[++i]);
            }
            catch (const std::exception&)
            {
                std::cerr << "Error: audio-delay must be a number from -1.0 to 1.0" << std::endl;
                return 1;
            }
        }
    }

    if (hasWidth != hasHeight)
    {
        std::cerr << "-W and -H must be provided together" << std::endl;
        return 1;
    }
    if (!hasWidth)
        size = getTerminalSize();

    if (size.width <= 0 || size.height <= 0)
    {
        std::cerr << "Error: width and height must be positive" << std::endl;
        return 1;
    }
    if (volume < 0.0f || volume > 2.0f)
    {
        std::cerr << "Error: volume must be from 0.0 to 2.0" << std::endl;
        return 1;
    }
    if (audioDelay < -1.0 || audioDelay > 1.0)
    {
        std::cerr << "Error: audio-delay must be from -1.0 to 1.0 seconds" << std::endl;
        return 1;
    }

    Renderer renderer(size.width, size.height*2);

    if (ftype == FILETYPE::INVALID)
    {
        std::cerr << "Input type error" << std::endl;
        return 1;
    }
    if (ftype == FILETYPE::IMAGE)
    {
        ImageSource img_src(path, size.width, size.height*2, cell_ratio);
        if (!img_src.isLoaded())
            return 1;

        std::vector<Color> pixels = img_src.getPixels();
        renderer.render(pixels);
        if (hasPath)
            renderer.save(outputPath.c_str(), pixels);

        std::cout << "\033[m";
        return 0;
    }
    if (ftype == FILETYPE::VIDEO)
    {
        g_src_ptr = new VideoSource(path, size.width, size.height*2, cell_ratio);
    }
    else if (ftype == FILETYPE::URL)
    {
        std::cout << "[ytoncmd] 正在透過串流技術接管影片數據...\n";
        g_src_ptr = new StreamSource(path, size.width, size.height*2, cell_ratio);
    }

    
    if (g_src_ptr != nullptr)
    {
        g_src_ptr->setVolume(volume);
        g_src_ptr->setAudioDelay(audioDelay);
        if (g_src_ptr->isReady())
        {
            std::cout << "\033[2J";
            renderer.play(*g_src_ptr);
        }
        else
            std::cerr << "[ytoncmd] 錯誤: 媒體來源未就緒，取消渲染播放。\n";

        delete g_src_ptr;
        g_src_ptr = nullptr;
    }

    if (g_interrupted != 0)
        std::cerr << "\n[ytoncmd] 已中斷播放並清理資源。\n";
    std::cout << "\033[m";
    return 0;
}
