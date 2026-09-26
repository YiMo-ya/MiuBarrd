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
	bool LoadVideo(std::vector<xs::IMAGE>& images, const std::wstring& path);
}