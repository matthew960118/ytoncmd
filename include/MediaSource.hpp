#ifndef MEDIASOURCE_HPP
#define MEDIASOURCE_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include "Color.hpp"
#include "miniaudio.h"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libswresample/swresample.h>
}

class MediaSource {
protected:
    struct AudioChunk {
        std::vector<uint8_t> pcm;
        size_t offset = 0;
        double pts_start = 0.0;
    };

    struct VideoFrame {
        std::vector<Color> pixels;
        double pts = 0.0;
    };

    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    
    int video_stream_idx = -1;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;
    std::vector<uint8_t> rgb_buffer;
    std::deque<VideoFrame> video_queue;

    int audio_stream_idx = -1;
    AVCodecContext* audio_codec_ctx = nullptr;
    SwrContext* swr_ctx = nullptr;

    std::deque<AudioChunk> audio_queue;
    mutable std::mutex audio_mutex;

    ma_device audio_device;
    bool m_audio_initialized = false;
    std::atomic<float> volume{1.0f};
    std::atomic<double> audio_delay{0.0};

    static constexpr int kAudioSampleRate = 48000;
    static constexpr int kAudioChannels = 2;
    static constexpr int kAudioBytesPerSample = 2;
    static constexpr int kAudioFrameBytes = kAudioChannels * kAudioBytesPerSample;

    static void maAudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void handleAudioPacket(AVPacket* packet);

    double fps = 30.0;
    int target_w, target_h;
    double ratio;
    int newW = 0, newH = 0;
    int offsetX = 0, offsetY = 0;

    std::string m_path;
    bool m_is_stream = false;

    std::atomic<double> current_video_pts{0.0};
    std::atomic<double> current_audio_pts{0.0};
    std::atomic<double> audio_play_clock{0.0};
    double video_time_base = 0.0;
    double audio_time_base = 0.0;

    bool initializeDecoder();
    void closeCurrentConnection();
    bool decodeAndQueueVideoFrame();
    bool readAndQueuePacket();
    bool popSyncedVideoFrame(std::vector<Color>& canvas);

public:
    MediaSource(const std::string& path, int tw, int th, double r, bool is_stream);
    virtual ~MediaSource();

    virtual bool open(const std::string& path) = 0;
    bool getNextFrame(std::vector<Color>& canvas);
    
    double getFPS() const { return fps; }
    bool isReady() const { return fmt_ctx != nullptr && codec_ctx != nullptr; }
    bool hasAudio() const { return audio_stream_idx != -1 && audio_codec_ctx != nullptr && m_audio_initialized; }

    void initAudioDevice();
    void setVolume(float value) { volume.store(value); }
    float getVolume() const { return volume.load(); }
    void setAudioDelay(double value) { audio_delay.store(value); }
    double getAudioDelay() const { return audio_delay.load(); }

    size_t getAudioBufferSize() const {
        std::lock_guard<std::mutex> lock(audio_mutex);
        size_t total = 0;
        for (const AudioChunk& chunk : audio_queue)
            total += chunk.pcm.size() - chunk.offset;
        return total;
    }

    double getVideoPts() const { return current_video_pts.load(); }
    double getAudioPts() const { return current_audio_pts.load(); }
    double getAudioClock() const { return audio_play_clock.load(); }
};

#endif
