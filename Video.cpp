#include "Video.h"

namespace VideoLoader
{
    NOXS; NOSTD;

    struct VideoPlayer
    {
        AVFormatContext* fmtCtx = nullptr;
        AVCodecContext* codecCtx = nullptr;
        SwsContext* swsCtx = nullptr;

        AVFrame* frame = nullptr;
        AVPacket* pkt = nullptr;

        int videoStream = -1;

        int w = 0;
        int h = 0;
        uint8_t* rgbaBuf = nullptr;

        bool eof = false;
    };

    static VideoPlayer g_Video;

    bool OpenVideo(const std::wstring& path)
    {
        CloseVideo();

        // wstring → UTF-8
        int len = WideCharToMultiByte(
            CP_UTF8, 0,
            path.c_str(), -1,
            nullptr, 0, nullptr, nullptr
        );

        std::string utf8Path(len, '\0');
        WideCharToMultiByte(
            CP_UTF8, 0,
            path.c_str(), -1,
            &utf8Path[0], len, nullptr, nullptr
        );

        if (avformat_open_input(&g_Video.fmtCtx, utf8Path.c_str(), nullptr, nullptr) != 0)
            return false;

        if (avformat_find_stream_info(g_Video.fmtCtx, nullptr) < 0)
        {
            avformat_close_input(&g_Video.fmtCtx);
            return false;
        }

        g_Video.videoStream = -1;
        for (unsigned i = 0; i < g_Video.fmtCtx->nb_streams; ++i)
        {
            if (g_Video.fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                g_Video.videoStream = i;
                break;
            }
        }

        if (g_Video.videoStream < 0)
        {
            avformat_close_input(&g_Video.fmtCtx);
            return false;
        }

        AVCodecParameters* codecPar =
            g_Video.fmtCtx->streams[g_Video.videoStream]->codecpar;

        const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
        if (!codec)
            return false;

        g_Video.codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(g_Video.codecCtx, codecPar);
        avcodec_open2(g_Video.codecCtx, codec, nullptr);

        g_Video.w = g_Video.codecCtx->width;
        g_Video.h = g_Video.codecCtx->height;

        g_Video.swsCtx = sws_getContext(
            g_Video.w, g_Video.h, g_Video.codecCtx->pix_fmt,
            g_Video.w, g_Video.h, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        int rgbaSize = av_image_get_buffer_size(
            AV_PIX_FMT_RGBA, g_Video.w, g_Video.h, 1
        );

        g_Video.rgbaBuf = (uint8_t*)av_malloc(rgbaSize);
        g_Video.frame = av_frame_alloc();
        g_Video.pkt = av_packet_alloc();

        g_Video.eof = false;
        return true;
    }

    bool DrawVideo(std::vector<IMAGE>& images)
    {
        if (!g_Video.fmtCtx || g_Video.eof)
            return false;

        while (true)
        {
            if (av_read_frame(g_Video.fmtCtx, g_Video.pkt) < 0)
            {
                g_Video.eof = true;
                return false;
            }

            if (g_Video.pkt->stream_index != g_Video.videoStream)
            {
                av_packet_unref(g_Video.pkt);
                continue;
            }

            avcodec_send_packet(g_Video.codecCtx, g_Video.pkt);
            av_packet_unref(g_Video.pkt);

            if (avcodec_receive_frame(g_Video.codecCtx, g_Video.frame) != 0)
                continue;

            uint8_t* dstData[1] = { g_Video.rgbaBuf };
            int dstLinesize[1] = { g_Video.w * 4 };

            sws_scale(
                g_Video.swsCtx,
                g_Video.frame->data,
                g_Video.frame->linesize,
                0,
                g_Video.h,
                dstData,
                dstLinesize
            );

            IMAGE img;
            img.w = g_Video.w;
            img.h = g_Video.h;
            img.color = sf::Color::White;

            img.texture.resize({ (unsigned)g_Video.w, (unsigned)g_Video.h });
            img.texture.update(g_Video.rgbaBuf);

            // 不清除旧数据，直接追加
            images.push_back(std::move(img));

            return true;
        }
    }

    void CloseVideo()
    {
        if (g_Video.pkt)
            av_packet_free(&g_Video.pkt);

        if (g_Video.frame)
            av_frame_free(&g_Video.frame);

        if (g_Video.rgbaBuf)
            av_free(g_Video.rgbaBuf);

        if (g_Video.swsCtx)
            sws_freeContext(g_Video.swsCtx);

        if (g_Video.codecCtx)
            avcodec_free_context(&g_Video.codecCtx);

        if (g_Video.fmtCtx)
            avformat_close_input(&g_Video.fmtCtx);

        g_Video = VideoPlayer{};
    }
}