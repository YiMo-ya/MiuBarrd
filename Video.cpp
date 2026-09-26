#include "Video.h"

namespace VideoLoader
{
    NOXS; NOSTD;

    bool LoadVideo(std::vector<IMAGE>& images, const std::wstring& path)
    {
        images.clear();

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

        AVFormatContext* fmtCtx = nullptr;
        if (avformat_open_input(&fmtCtx, utf8Path.c_str(), nullptr, nullptr) != 0)
            return false;

        if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
        {
            avformat_close_input(&fmtCtx);
            return false;
        }

        int videoStream = -1;
        for (unsigned i = 0; i < fmtCtx->nb_streams; ++i)
        {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                videoStream = i;
                break;
            }
        }

        if (videoStream < 0)
        {
            avformat_close_input(&fmtCtx);
            return false;
        }

        AVCodecParameters* codecPar = fmtCtx->streams[videoStream]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
        if (!codec)
            return false;

        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecCtx, codecPar);
        avcodec_open2(codecCtx, codec, nullptr);

        int w = codecCtx->width;
        int h = codecCtx->height;

        SwsContext* swsCtx = sws_getContext(
            w, h, codecCtx->pix_fmt,
            w, h, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        int rgbaSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, w, h, 1);
        uint8_t* rgbaBuf = (uint8_t*)av_malloc(rgbaSize);

        AVFrame* frame = av_frame_alloc();
        AVPacket* pkt = av_packet_alloc();

        while (av_read_frame(fmtCtx, pkt) >= 0)
        {
            if (pkt->stream_index == videoStream)
            {
                avcodec_send_packet(codecCtx, pkt);
                while (avcodec_receive_frame(codecCtx, frame) == 0)
                {
                    // 转 RGBA
                    uint8_t* dstData[1] = { rgbaBuf };
                    int dstLinesize[1] = { w * 4 };

                    sws_scale(
                        swsCtx,
                        frame->data,
                        frame->linesize,
                        0,
                        h,
                        dstData,
                        dstLinesize
                    );

                    IMAGE img;
                    img.w = w;
                    img.h = h;
                    img.color = sf::Color::White;

                    // 必须 create
                    img.texture.resize({ (unsigned)w, (unsigned)h });
                    img.texture.update(rgbaBuf);

                    images.push_back(std::move(img));
                }
            }
            av_packet_unref(pkt);
        }

        // 清理
        av_packet_free(&pkt);
        av_frame_free(&frame);
        av_free(rgbaBuf);
        sws_freeContext(swsCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);

        return !images.empty();
    }

}