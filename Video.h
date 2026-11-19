#pragma once
#include "Xs/Xs.h"

extern "C" {
#include "ffmpeg/include/libavformat/avformat.h"
#include "ffmpeg/include/libavcodec/avcodec.h"
#include "ffmpeg/include/libswscale/swscale.h"
#include "ffmpeg/include/libavutil/imgutils.h"
}

namespace VideoLoader
{
    // 打开视频（只需调用一次）
    bool OpenVideo(const std::wstring& path);

    // 每帧调用：解码一帧并追加到 images（不清除原数据）
    // 返回 false 表示视频结束或尚未打开
    bool DrawVideo(std::vector<xs::IMAGE>& images);

    // 关闭并释放资源（结束时调用，可重复调用）
    void CloseVideo();
}