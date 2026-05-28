#include "MediaSource.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

extern "C"
{
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
}

static AVPixelFormat selectSoftwarePixelFormat(AVCodecContext *, const AVPixelFormat *pixelFormats)
{
    for (const AVPixelFormat *format = pixelFormats; *format != AV_PIX_FMT_NONE; ++format)
    {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*format);
        if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
            return *format;
    }

    return pixelFormats[0];
}

MediaSource::MediaSource(const std::string &path, int tw, int th, double r, bool is_stream)
    : m_path(path), target_w(tw), target_h(th), ratio(r), m_is_stream(is_stream) {}

MediaSource::~MediaSource()
{
    closeCurrentConnection();
}

void MediaSource::closeCurrentConnection()
{
    if (frame)
        av_frame_free(&frame);
    if (pkt)
        av_packet_free(&pkt);
    if (sws_ctx)
        sws_freeContext(sws_ctx);
    sws_ctx = nullptr;
    if (codec_ctx)
        avcodec_free_context(&codec_ctx);
    if (fmt_ctx)
        avformat_close_input(&fmt_ctx);

    video_stream_idx = -1;
    audio_stream_idx = -1;
    video_queue.clear();

    if (m_audio_initialized)
    {
        ma_device_uninit(&audio_device);
        m_audio_initialized = false;
    }
    if (swr_ctx)
        swr_free(&swr_ctx);
    if (audio_codec_ctx)
        avcodec_free_context(&audio_codec_ctx);

    {
        std::lock_guard<std::mutex> lock(audio_mutex);
        audio_queue.clear();
    }
    current_video_pts.store(0.0);
    current_audio_pts.store(0.0);
    audio_play_clock.store(0.0);
}

bool MediaSource::initializeDecoder()
{
    video_stream_idx = -1;
    audio_stream_idx = -1;

    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        AVMediaType type = fmt_ctx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1)
            video_stream_idx = i;
        if (type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1)
            audio_stream_idx = i;
    }

    if (video_stream_idx == -1)
    {
        std::cerr << "[FFmpeg] 錯誤: 找不到視訊流。\n";
        return false;
    }

    AVStream *v_stream = fmt_ctx->streams[video_stream_idx];
    video_time_base = av_q2d(v_stream->time_base);

    const AVCodec *codec = avcodec_find_decoder(v_stream->codecpar->codec_id);
    if (!codec)
        return false;

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
        return false;
    if (avcodec_parameters_to_context(codec_ctx, v_stream->codecpar) < 0)
        return false;
    codec_ctx->get_format = selectSoftwarePixelFormat;
    if (avcodec_open2(codec_ctx, codec, NULL) < 0)
        return false;

    int origW = codec_ctx->width;
    int origH = codec_ctx->height;
    if (origW <= 0 || origH <= 0)
    {
        std::cerr << "[FFmpeg] 警告: 串流寬高不合法，使用 1280x720 保底。\n";
        origW = 1280;
        origH = 720;
    }

    // float fontW = 9.0f, fontH = 19.0f;
    float scaleX = static_cast<float>(target_w) / origW;
    float scaleY = static_cast<float>(target_h) / (origH * ratio);
    float scale = std::min(scaleX, scaleY);

    newW = std::max(1, static_cast<int>(origW * scale));
    newH = std::max(1, static_cast<int>(origH * scale * ratio));
    newW = std::min(newW, target_w);
    newH = std::min(newH, target_h);

    rgb_buffer.resize(newW * newH * 3);

    if (v_stream->avg_frame_rate.den != 0)
        fps = av_q2d(v_stream->avg_frame_rate);
    if (fps <= 0.0)
        fps = 30.0;

    offsetX = (target_w - newW) / 2;
    offsetY = (target_h - newH) / 2;

    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if (!frame || !pkt)
        return false;

    if (audio_stream_idx == -1)
        return true;

    AVStream *a_stream = fmt_ctx->streams[audio_stream_idx];
    audio_time_base = av_q2d(a_stream->time_base);
    const AVCodec *audio_codec = avcodec_find_decoder(a_stream->codecpar->codec_id);
    if (!audio_codec)
        return true;

    audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_codec_ctx)
        return true;
    if (avcodec_parameters_to_context(audio_codec_ctx, a_stream->codecpar) < 0)
        return true;
    if (avcodec_open2(audio_codec_ctx, audio_codec, NULL) < 0)
        return true;

    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, 2);

    swr_alloc_set_opts2(
        &swr_ctx,
        &out_ch_layout, AV_SAMPLE_FMT_S16, kAudioSampleRate,
        &audio_codec_ctx->ch_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate,
        0, NULL);

    if (swr_ctx && swr_init(swr_ctx) >= 0)
        initAudioDevice();

    av_channel_layout_uninit(&out_ch_layout);
    return true;
}

bool MediaSource::readAndQueuePacket()
{
    int ret = av_read_frame(fmt_ctx, pkt);
    if (ret < 0)
    {
        av_packet_unref(pkt);
        return false;
    }

    if (pkt->stream_index == audio_stream_idx)
    {
        handleAudioPacket(pkt);
    }
    else if (pkt->stream_index == video_stream_idx)
    {
        decodeAndQueueVideoFrame();
    }

    av_packet_unref(pkt);
    return true;
}

bool MediaSource::decodeAndQueueVideoFrame()
{   
    av_log_set_level(AV_LOG_ERROR);
    if (avcodec_send_packet(codec_ctx, pkt) < 0)
        return false;

    bool queued = false;
    while (true)
    {
        int ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0)
            break;

        double pts = current_video_pts.load() + (1.0 / fps);
        if (frame->pts != AV_NOPTS_VALUE)
            pts = frame->pts * video_time_base;
        current_video_pts.store(pts);

        sws_ctx = sws_getCachedContext(
            sws_ctx,
            frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
            newW, newH, AV_PIX_FMT_RGB24,
            SWS_BICUBIC, NULL, NULL, NULL);
        if (!sws_ctx)
            break;

        uint8_t *dest[4] = {rgb_buffer.data(), NULL, NULL, NULL};
        int dest_linesize[4] = {newW * 3, 0, 0, 0};
        sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, dest, dest_linesize);

        VideoFrame out;
        out.pts = pts;
        out.pixels.assign(target_w * target_h, Color{0, 0, 0});

        for (int y = 0; y < newH; ++y)
        {
            for (int x = 0; x < newW; ++x)
            {
                int srcIdx = (y * newW + x) * 3;
                int tarIdx = (y + offsetY) * target_w + (x + offsetX);
                if (tarIdx >= 0 && tarIdx < static_cast<int>(out.pixels.size()))
                    out.pixels[tarIdx] = Color(rgb_buffer[srcIdx], rgb_buffer[srcIdx + 1], rgb_buffer[srcIdx + 2]);
            }
        }

        video_queue.push_back(std::move(out));
        queued = true;
    }

    return queued;
}

bool MediaSource::popSyncedVideoFrame(std::vector<Color>& canvas)
{
    if (video_queue.empty())
        return false;

    if (!hasAudio())
    {
        canvas = std::move(video_queue.front().pixels);
        current_video_pts.store(video_queue.front().pts);
        video_queue.pop_front();
        return true;
    }

    double audioClock = audio_play_clock.load();
    if (audioClock <= 0.0)
        return false;

    double targetClock = audioClock - audio_delay.load();
    constexpr double lateDropThreshold = 0.060;
    constexpr double earlyRenderTolerance = 0.012;

    while (video_queue.size() > 1 && video_queue[1].pts < targetClock - lateDropThreshold)
        video_queue.pop_front();

    double lead = video_queue.front().pts - targetClock;
    if (lead > earlyRenderTolerance)
        return false;

    canvas = std::move(video_queue.front().pixels);
    current_video_pts.store(video_queue.front().pts);
    video_queue.pop_front();
    return true;
}

bool MediaSource::getNextFrame(std::vector<Color> &canvas)
{
    const size_t targetAudioBytes = static_cast<size_t>(kAudioSampleRate * kAudioFrameBytes * 0.18);

    while (true)
    {
        while (video_queue.size() < 4 || (hasAudio() && getAudioBufferSize() < targetAudioBytes))
        {
            if (!readAndQueuePacket())
                break;
            if (video_queue.size() >= 4 && (!hasAudio() || getAudioBufferSize() >= targetAudioBytes))
                break;
        }

        if (popSyncedVideoFrame(canvas))
            return true;

        if (video_queue.empty())
            return false;

        if (!hasAudio())
            continue;

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void MediaSource::handleAudioPacket(AVPacket *packet)
{
    if (!audio_codec_ctx || !swr_ctx)
        return;

    if (avcodec_send_packet(audio_codec_ctx, packet) < 0)
        return;

    AVFrame *audio_frame = av_frame_alloc();
    if (!audio_frame)
        return;

    while (avcodec_receive_frame(audio_codec_ctx, audio_frame) >= 0)
    {
        double frameStart = current_audio_pts.load();
        if (audio_frame->pts != AV_NOPTS_VALUE)
            frameStart = audio_frame->pts * audio_time_base;

        int out_samples = av_rescale_rnd(
            swr_get_delay(swr_ctx, audio_codec_ctx->sample_rate) + audio_frame->nb_samples,
            kAudioSampleRate, audio_codec_ctx->sample_rate, AV_ROUND_UP);

        std::vector<uint8_t> pcm_output(out_samples * kAudioFrameBytes);
        uint8_t *dst_data[1] = {pcm_output.data()};
        int converted = swr_convert(
            swr_ctx, dst_data, out_samples,
            (const uint8_t **)audio_frame->data, audio_frame->nb_samples);

        if (converted > 0)
        {
            AudioChunk chunk;
            chunk.pts_start = frameStart;
            chunk.pcm.resize(converted * kAudioFrameBytes);
            std::copy(pcm_output.begin(), pcm_output.begin() + chunk.pcm.size(), chunk.pcm.begin());

            current_audio_pts.store(chunk.pts_start + static_cast<double>(converted) / kAudioSampleRate);

            std::lock_guard<std::mutex> lock(audio_mutex);
            audio_queue.push_back(std::move(chunk));
        }
    }

    av_frame_free(&audio_frame);
}

void MediaSource::initAudioDevice()
{
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = kAudioChannels;
    config.sampleRate = kAudioSampleRate;
    config.dataCallback = MediaSource::maAudioCallback;
    config.pUserData = this;

    if (ma_device_init(NULL, &config, &audio_device) == MA_SUCCESS)
    {
        ma_device_start(&audio_device);
        m_audio_initialized = true;
    }
}

void MediaSource::maAudioCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    (void)pInput;
    MediaSource *src = static_cast<MediaSource *>(pDevice->pUserData);

    size_t bytesNeeded = frameCount * kAudioFrameBytes;
    uint8_t *outBuf = static_cast<uint8_t *>(pOutput);
    size_t written = 0;
    double latestClock = src->audio_play_clock.load();

    {
        std::lock_guard<std::mutex> lock(src->audio_mutex);
        while (written < bytesNeeded && !src->audio_queue.empty())
        {
            AudioChunk& chunk = src->audio_queue.front();
            size_t available = chunk.pcm.size() - chunk.offset;
            size_t toCopy = std::min(bytesNeeded - written, available);

            std::memcpy(outBuf + written, chunk.pcm.data() + chunk.offset, toCopy);
            chunk.offset += toCopy;
            written += toCopy;

            size_t playedFramesInChunk = chunk.offset / kAudioFrameBytes;
            latestClock = chunk.pts_start + static_cast<double>(playedFramesInChunk) / kAudioSampleRate;

            if (chunk.offset >= chunk.pcm.size())
                src->audio_queue.pop_front();
        }
    }

    if (written < bytesNeeded)
        std::fill(outBuf + written, outBuf + bytesNeeded, 0);

    if (latestClock > 0.0)
        src->audio_play_clock.store(latestClock);

    float gain = src->volume.load();
    if (gain == 1.0f)
        return;

    for (size_t i = 0; i + 1 < bytesNeeded; i += sizeof(int16_t))
    {
        int16_t sample = 0;
        std::memcpy(&sample, outBuf + i, sizeof(sample));
        int scaled = static_cast<int>(sample * gain);
        scaled = std::clamp(scaled, -32768, 32767);
        sample = static_cast<int16_t>(scaled);
        std::memcpy(outBuf + i, &sample, sizeof(sample));
    }
}
