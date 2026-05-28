#include <iostream>

void printHelp() {
    std::cout << "======================================================================\n";
    std::cout << "  ytoncmd - YouTube / Video or Image to Command Line Renderer (v1.3.0)\n";
    std::cout << "======================================================================\n\n";
    
    std::cout << "USAGE:\n";
    std::cout << "  ytoncmd <path/url> [options]\n\n";
    
    std::cout << "ARGUMENTS:\n";
    std::cout << "  <path/url>   圖片、影片的檔案路徑（如 ./video.mp4）或網路 URL\n\n";
    
    std::cout << "OPTIONS:\n";
    std::cout << "  -h, --help              顯示此幫助訊息並退出\n";
    std::cout << "  -W, --width <width>     指定渲染寬度 (預設: 自動適應終端機寬度)\n";
    std::cout << "  -H, --height <height>   指定渲染高度 (預設: 自動適應終端機高度)\n";
    std::cout << "  -r, --cell-ratio <val>  指定字元寬高比 (寬/高，預設: 0.94(18/19)，適用 9/19 字體(字體的上半為一像素))\n";
    std::cout << "  -o, --output <path>     指定圖片 ANSI 輸出路徑\n";
    std::cout << "  -v, --volume <value>    指定播放音量倍率，0.0 靜音，1.0 原音量，2.0 最大\n";
    std::cout << "  --audio-delay <sec>     音訊延遲補償；正數代表聲音慢，會延後畫面\n\n";
    
    std::cout << "EXAMPLES:\n";
    std::cout << "  ytoncmd ./bad_apple.mp4\n";
    std::cout << "  ytoncmd ./image.png -W 80 -H 40 -o output.txt\n";
    std::cout << "  ytoncmd http://example.com/stream.m3u8 -W 80 -H 40 -v 0.8 --audio-delay 0.12\n\n";
    
    std::cout << "----------------------------------------------------------------------\n";
    std::cout << "提示: 播放時按下 Q 鍵可退出。\n";
}
