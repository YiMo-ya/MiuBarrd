extern "C" {
#include "ffmpeg/include/libavformat/avformat.h"
#include "ffmpeg/include/libavcodec/avcodec.h"
#include "ffmpeg/include/libavdevice/avdevice.h"
#include "ffmpeg/include/libswscale/swscale.h"
#include "ffmpeg/include/libavutil/avutil.h"
#include "ffmpeg/include/libavutil/imgutils.h"
#include "ffmpeg/include/libavutil/time.h"
}

#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")

#include <dshow.h>
#include <dvdmedia.h>
#include <windows.h>
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")

#include "Camera.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <utility>
#include <cstring>
#include <cstdlib>
#include "Shared.h"
#include "Write.h"

namespace cam {

#ifdef _WIN32
    // 通过 DirectShow 枚举指定摄像头实际支持的视频模式（宽 x 高）。
    // deviceName：设备友好名称（不含 "video=" 前缀）。
    // 返回值已按分辨率去重；枚举失败或设备不支持时返回空列表。
    static std::vector<std::pair<int, int>> enumerateDshowModes(const std::string& deviceName) {
        std::vector<std::pair<int, int>> modes;

        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        ICreateDevEnum* pDevEnum = nullptr;
        if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC,
            IID_ICreateDevEnum, (void**)&pDevEnum)) || !pDevEnum) {
            if (hrInit == S_OK) CoUninitialize();
            return modes;
        }

        IEnumMoniker* pEnum = nullptr;
        if (FAILED(pDevEnum->CreateClassEnumerator(
            CLSID_VideoInputDeviceCategory, &pEnum, 0)) || !pEnum) {
            pDevEnum->Release();
            if (hrInit == S_OK) CoUninitialize();
            return modes;
        }

        IMoniker* pMoniker = nullptr;
        while (pEnum->Next(1, &pMoniker, nullptr) == S_OK) {
            // 读取设备友好名称，只处理与目标设备同名的那一个
            std::string name;
            IPropertyBag* pBag = nullptr;
            if (SUCCEEDED(pMoniker->BindToStorage(nullptr, nullptr,
                IID_IPropertyBag, (void**)&pBag)) && pBag) {
                VARIANT var;
                VariantInit(&var);
                if (SUCCEEDED(pBag->Read(L"FriendlyName", &var, nullptr))) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1,
                        nullptr, 0, nullptr, nullptr);
                    name.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1,
                        name.data(), len, nullptr, nullptr);
                    VariantClear(&var);
                }
                pBag->Release();
            }

            if (name == deviceName) {
                IBaseFilter* pCap = nullptr;
                if (SUCCEEDED(pMoniker->BindToObject(nullptr, nullptr,
                    IID_IBaseFilter, (void**)&pCap)) && pCap) {

                    IGraphBuilder* pGraph = nullptr;
                    ICaptureGraphBuilder2* pBuilder = nullptr;
                    if (SUCCEEDED(CoCreateInstance(CLSID_FilterGraph, nullptr,
                            CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&pGraph)) &&
                        SUCCEEDED(CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr,
                            CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&pBuilder))) {

                        pBuilder->SetFiltergraph(pGraph);
                        pGraph->AddFilter(pCap, L"Capture Filter");

                        // 通过捕获管脚的 IAMStreamConfig 拿到设备支持的全部能力项
                        IAMStreamConfig* pConfig = nullptr;
                        if (SUCCEEDED(pBuilder->FindInterface(&PIN_CATEGORY_CAPTURE,
                                &MEDIATYPE_Video, pCap, IID_IAMStreamConfig, (void**)&pConfig))
                            && pConfig) {

                            int count = 0, size = 0;
                            if (SUCCEEDED(pConfig->GetNumberOfCapabilities(&count, &size))) {
                                std::vector<BYTE> buf(static_cast<size_t>(size));
                                for (int i = 0; i < count; ++i) {
                                    AM_MEDIA_TYPE* pmt = nullptr;
                                    if (SUCCEEDED(pConfig->GetStreamCaps(i, &pmt, buf.data())) && pmt) {
                                        if ((pmt->formattype == FORMAT_VideoInfo ||
                                             pmt->formattype == FORMAT_VideoInfo2) && pmt->pbFormat) {
                                            const BITMAPINFOHEADER* bih =
                                                (pmt->formattype == FORMAT_VideoInfo2)
                                                ? &reinterpret_cast<VIDEOINFOHEADER2*>(pmt->pbFormat)->bmiHeader
                                                : &reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat)->bmiHeader;
                                            int w = bih->biWidth;
                                            int h = std::abs(bih->biHeight);
                                            if (w > 0 && h > 0) {
                                                std::pair<int, int> m(w, h);
                                                if (std::find(modes.begin(), modes.end(), m) == modes.end())
                                                    modes.push_back(m);
                                            }
                                        }
                                        if (pmt->pUnk) pmt->pUnk->Release();
                                        if (pmt->pbFormat) CoTaskMemFree(pmt->pbFormat);
                                        CoTaskMemFree(pmt);
                                    }
                                }
                            }
                            pConfig->Release();
                        }

                        pGraph->RemoveFilter(pCap);
                        pBuilder->Release();
                        pGraph->Release();
                    }
                    pCap->Release();
                }
            }
            pMoniker->Release();
        }

        pEnum->Release();
        pDevEnum->Release();
        if (hrInit == S_OK) CoUninitialize();

        return modes;
    }

    // 从设备支持的模式列表中挑选与请求分辨率最接近的一个：
    // 精确匹配直接返回；否则以「宽、高差绝对值之和」为距离取最小者。
    // 列表为空时返回 (0,0)，调用方据此保持原请求分辨率。
    static std::pair<int, int> pickClosestMode(
        const std::vector<std::pair<int, int>>& modes, int reqW, int reqH) {
        if (modes.empty()) return std::make_pair(0, 0);

        std::pair<int, int> best(0, 0);
        long long bestScore = -1;
        for (size_t i = 0; i < modes.size(); ++i) {
            const std::pair<int, int>& m = modes[i];
            if (m.first == reqW && m.second == reqH)
                return m;
            long long score = std::llabs(static_cast<long long>(m.first) - reqW)
                            + std::llabs(static_cast<long long>(m.second) - reqH);
            if (bestScore < 0 || score < bestScore) {
                bestScore = score;
                best = m;
            }
        }
        return best;
    }
#endif

    std::vector<DeviceInfo> Camera::listDevices() {
        std::vector<DeviceInfo> devices;

#ifdef _WIN32
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ICreateDevEnum* pDevEnum = nullptr;
        IEnumMoniker* pEnum = nullptr;

        if (SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
            CLSCTX_INPROC, IID_ICreateDevEnum, (void**)&pDevEnum)) &&
            SUCCEEDED(pDevEnum->CreateClassEnumerator(
                CLSID_VideoInputDeviceCategory, &pEnum, 0))) {

            IMoniker* pMoniker = nullptr;
            int index = 0;
            while (pEnum->Next(1, &pMoniker, nullptr) == S_OK) {
                IPropertyBag* pBag = nullptr;
                if (SUCCEEDED(pMoniker->BindToStorage(nullptr, nullptr,
                    IID_IPropertyBag, (void**)&pBag))) {

                    VARIANT var;
                    VariantInit(&var);

                    DeviceInfo info;
                    info.index = index;

                    if (SUCCEEDED(pBag->Read(L"FriendlyName", &var, nullptr))) {
                        int len = WideCharToMultiByte(CP_UTF8, 0,
                            var.bstrVal, -1, nullptr, 0, nullptr, nullptr);
                        info.name.resize(len - 1);
                        WideCharToMultiByte(CP_UTF8, 0,
                            var.bstrVal, -1,
                            info.name.data(), len, nullptr, nullptr);
                        VariantClear(&var);
                    }

                    info.id = "video=" + info.name;
                    devices.push_back(info);
                    ++index;
                    pBag->Release();
                }
                pMoniker->Release();
            }
            pEnum->Release();
        }
        if (pDevEnum) pDevEnum->Release();
        CoUninitialize();

#elif defined(__APPLE__)
        for (int i = 0; i < 8; ++i)
            devices.push_back({ i, "Camera " + std::to_string(i), std::to_string(i) });
#else
        for (int i = 0; i < 16; ++i) {
            std::string path = "/dev/video" + std::to_string(i);
            if (access(path.c_str(), R_OK) == 0)
                devices.push_back({ i, "Video " + std::to_string(i), path });
        }
#endif
        return devices;
    }

    CameraConfig Camera::makeConfig(int index, int width, int height, int fps) {
        CameraConfig cfg;
        cfg.width = width;
        cfg.height = height;
        cfg.fps = fps;

        auto devs = listDevices();
        if (devs.empty()) {
#if   defined(_WIN32)
            cfg.device = "video=USB Camera";
            cfg.format = "dshow";
#elif defined(__APPLE__)
            cfg.device = "0";
            cfg.format = "avfoundation";
#else
            cfg.device = "/dev/video0";
            cfg.format = "v4l2";
#endif
            return cfg;
        }

        int i = (index < 0 || index >= static_cast<int>(devs.size()))
            ? static_cast<int>(devs.size()) - 1 : index;

        cfg.device = devs[i].id;
#if   defined(_WIN32)
        cfg.format = "dshow";
#elif defined(__APPLE__)
        cfg.format = "avfoundation";
#else
        cfg.format = "v4l2";
#endif
        return cfg;
    }

    // ============================================================
    //  PImpl
    // ============================================================
    struct Camera::Impl {
        CameraConfig   cfg;
        AVFormatContext* fmtCtx = nullptr;
        AVCodecContext* codecCtx = nullptr;
        SwsContext* sws = nullptr;
        int              videoStream = -1;

        std::atomic<bool> running{ false };
        std::thread       worker;

        std::mutex        frameMutex;
        std::vector<uint8_t> frontBuf;
        std::vector<uint8_t> backBuf;
        int                frameW = 0;
        int                frameH = 0;

        std::atomic<bool>  hasNew{ false };   // 原子化

        explicit Impl(const CameraConfig& c) : cfg(c) {}
        ~Impl() { stop(); }

        void stop() {
            running = false;
            if (worker.joinable()) worker.join();
            sws_freeContext(sws); sws = nullptr;
            avcodec_free_context(&codecCtx);
            avformat_close_input(&fmtCtx);
            frontBuf.clear();
            backBuf.clear();
        }

        void captureLoop() {
            AVPacket* pkt = av_packet_alloc();
            AVFrame* frame = av_frame_alloc();
            if (!pkt || !frame) {
                if (pkt)  av_packet_free(&pkt);
                if (frame) av_frame_free(&frame);
                return;
            }

            AVFrame* rgbaFrame = av_frame_alloc();
            if (rgbaFrame) {
                rgbaFrame->format = AV_PIX_FMT_RGBA;
                rgbaFrame->width = frameW;
                rgbaFrame->height = frameH;
                av_frame_get_buffer(rgbaFrame, 0);
            }

            while (running) {
                int ret = av_read_frame(fmtCtx, pkt);
                if (ret < 0) {
                    av_usleep(1000);
                    continue;
                }

                if (pkt->stream_index == videoStream) {
                    if (avcodec_send_packet(codecCtx, pkt) == 0) {
                        while (avcodec_receive_frame(codecCtx, frame) == 0) {
                            sws_scale(sws,
                                frame->data, frame->linesize,
                                0, frameH,
                                rgbaFrame->data, rgbaFrame->linesize);

                            {
                                std::lock_guard<std::mutex> lock(frameMutex);
                                for (int y = 0; y < frameH; ++y) {
                                    uint8_t* dst = backBuf.data() + y * frameW * 4;
                                    uint8_t* src = rgbaFrame->data[0] + y * rgbaFrame->linesize[0];
                                    std::memcpy(dst, src, static_cast<size_t>(frameW * 4));
                                }
                                std::swap(frontBuf, backBuf);
                                hasNew = true;   // 写端 release
                            }
                        }
                    }
                }
                av_packet_unref(pkt);
            }

            if (rgbaFrame) av_frame_free(&rgbaFrame);
            av_frame_free(&frame);
            av_packet_free(&pkt);
        }
    };

    // ------------------ set / apply ------------------
    bool Camera::setDevice(int index) {
        Impl& d = *p_;
        auto devs = listDevices();
        if (devs.empty()) return false;

        int i = (index < 0 || index >= static_cast<int>(devs.size()))
            ? static_cast<int>(devs.size()) - 1 : index;

        CameraConfig newCfg = d.cfg;
        newCfg.device = devs[i].id;
        return applyConfig(newCfg);
    }

    bool Camera::setWidth(int width) {
        if (width <= 0) return false;
        CameraConfig c = p_->cfg;
        c.width = width;
        return applyConfig(c);
    }

    bool Camera::setHeight(int height) {
        if (height <= 0) return false;
        CameraConfig c = p_->cfg;
        c.height = height;
        return applyConfig(c);
    }

    bool Camera::applyConfig(const CameraConfig& newCfg) {
        Impl& d = *p_;
        if (newCfg.device == d.cfg.device &&
            newCfg.format == d.cfg.format &&
            newCfg.width == d.cfg.width &&
            newCfg.height == d.cfg.height &&
            newCfg.fps == d.cfg.fps)
            return true;

        CameraConfig oldCfg = d.cfg;
        stop();
        d.cfg = newCfg;
        if (start()) return true;
        d.cfg = oldCfg;
        start();
        return false;
    }

    // ------------------ 生命周期 ------------------
    Camera::Camera(const CameraConfig& cfg) : p_(new Impl(cfg)) {}
    Camera::~Camera() { stop(); delete p_; }

    bool Camera::start() {
        Impl& d = *p_;
        if (d.running) return true;

        avdevice_register_all();
        avformat_network_init();

        const AVInputFormat* ifmt = av_find_input_format(d.cfg.format.c_str());
        if (!ifmt) return false;

        AVDictionary* opts = nullptr;

        // 先在设备真实支持的分辨率中挑选与请求值最接近的模式，
        // 避免请求分辨率不被驱动支持时静默回落到默认的最低分辨率。
        int reqW = d.cfg.width;
        int reqH = d.cfg.height;
#ifdef _WIN32
        if (d.cfg.format == "dshow") {
            std::string devName = d.cfg.device;
            const std::string kPrefix = "video=";
            if (devName.rfind(kPrefix, 0) == 0)
                devName = devName.substr(kPrefix.size());

            std::vector<std::pair<int, int>> modes = enumerateDshowModes(devName);
            std::pair<int, int> best = pickClosestMode(modes, reqW, reqH);
            if (best.first > 0 && best.second > 0) {
                reqW = best.first;
                reqH = best.second;
            }
        }
#endif

        const std::string sizeStr = std::to_string(reqW) + "x" + std::to_string(reqH);
        const std::string fpsStr = std::to_string(d.cfg.fps);

        av_dict_set(&opts, "video_size", sizeStr.c_str(), 0);
        av_dict_set(&opts, "framerate", fpsStr.c_str(), 0);
        // dshow 未指定像素格式时会默认协商未压缩的 YUY2，高分辨率下带宽不足会被驱动回退；
        // 因此优先请求 MJPEG，若该模式不支持 MJPEG 再回退到默认像素格式。
        av_dict_set(&opts, "pixel_format", "mjpeg", 0);

        if (avformat_open_input(&d.fmtCtx, d.cfg.device.c_str(), ifmt, &opts) != 0) {
            // MJPEG 协商失败则改用默认像素格式重试，避免设备直接打不开。
            av_dict_free(&opts);
            opts = nullptr;
            av_dict_set(&opts, "video_size", sizeStr.c_str(), 0);
            av_dict_set(&opts, "framerate", fpsStr.c_str(), 0);
            if (avformat_open_input(&d.fmtCtx, d.cfg.device.c_str(), ifmt, &opts) != 0) {
                av_dict_free(&opts);
                return false;
            }
        }
        av_dict_free(&opts);

        if (avformat_find_stream_info(d.fmtCtx, nullptr) < 0) {
            avformat_close_input(&d.fmtCtx);
            return false;
        }

        for (unsigned i = 0; i < d.fmtCtx->nb_streams; ++i) {
            if (d.fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                d.videoStream = static_cast<int>(i);
                break;
            }
        }
        if (d.videoStream < 0) {
            avformat_close_input(&d.fmtCtx);
            return false;
        }

        AVCodecParameters* par = d.fmtCtx->streams[d.videoStream]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(par->codec_id);
        if (!codec) {
            avformat_close_input(&d.fmtCtx);
            return false;
        }

        d.codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(d.codecCtx, par);
        if (avcodec_open2(d.codecCtx, codec, nullptr) < 0) {
            avcodec_free_context(&d.codecCtx);
            avformat_close_input(&d.fmtCtx);
            return false;
        }

        d.frameW = d.codecCtx->width;
        d.frameH = d.codecCtx->height;

        d.sws = sws_getContext(
            d.frameW, d.frameH, d.codecCtx->pix_fmt,
            d.frameW, d.frameH, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!d.sws) {
            avcodec_free_context(&d.codecCtx);
            avformat_close_input(&d.fmtCtx);
            return false;
        }

        d.frontBuf.resize(static_cast<size_t>(d.frameW * d.frameH * 4));
        d.backBuf.resize(static_cast<size_t>(d.frameW * d.frameH * 4));

        d.running = true;
        d.worker = std::thread([this]() { p_->captureLoop(); });
        return true;
    }

    void Camera::stop() 
    { 
        if (p_) p_->stop(); 
    }

    bool Camera::isRunning() const { return p_ && p_->running.load(); }
    int  Camera::width()  const { return p_ ? p_->frameW : 0; }
    int  Camera::height() const { return p_ ? p_->frameH : 0; }

    // 关键修复点
    bool Camera::update(sf::Texture& tex) {
        Impl& d = *p_;
        if (!d.hasNew.load(std::memory_order_acquire))
            return false;

        std::lock_guard<std::mutex> lock(d.frameMutex);

        sf::Vector2u ts = tex.getSize();
        if (ts.x != static_cast<unsigned>(d.frameW) ||
            ts.y != static_cast<unsigned>(d.frameH)) {
            tex.resize({ unsigned int(d.frameW), unsigned int(d.frameH) });   // ✅ 必须 create，不能 resize
        }

        tex.update(d.frontBuf.data());
        d.hasNew.store(false, std::memory_order_release);
        return true;
    }

} // namespace cam

// ============================================================
//  全局 & Manager
// ============================================================
using namespace cam;

Camera Cam(Cam.makeConfig(0));
static int CameraIndex = -1;

xs::EV CameraManager::Rotation;

int SleepUpdateTime = 0;
static xs::IMAGE tex;
void CameraManager::DrawCameraImage(int x, int y, float Scale, sf::RenderWindow& window) {

    static bool IsInit = false;
    if (!IsInit)
    {
        Rotation.SetAnimationStartValue(-90);
        Rotation.end = -90;
        IsInit = true;
    }

    if (!Cam.isRunning()) return;

    Rotation.UpdateAnimation(xs::XEase::EaseBasic::easeOut, 4);
    if (Rotation.value <= -360)
    {
        Rotation.value = 0;
    }

    // 仅在尺寸变化时才写入宽高，避免每帧赋值（尺寸不变时无意义）
    tex.w = Cam.width();
    tex.h = Cam.height();

    if (SleepUpdateTime <= 0) Cam.update(tex.texture);
    else SleepUpdateTime -= 1;

    tex.color = Color(220, 220, 220);

    xs::XImage::PutRoteScaleImage(tex, x, y, Rotation.value, Scale, Scale, window, 0.5, 0.5);
}

bool CameraManager::Init(int index, int w, int h) {
    CameraIndex = index;
    if (!Cam.applyConfig(Cam.makeConfig(index, w, h)))
    {
        CameraIndex = -1;

        return false;
    }
    else
    {
        if (!Cam.start()) return false;
    }

    return true;
}

void CameraManager::Stop() {
    Cam.stop();
}

void CameraManager::SleepUpdate(int time)
{
    SleepUpdateTime = time;
}

int CameraManager::GetW()
{
    return Cam.width();
}

int CameraManager::GetH()
{
    return Cam.height();
}

void CameraManager::Get(xs::IMAGE& img)
{
    NOXS;NOSTD;

    // 直接从显存回读到位图，避免经磁盘 PNG 文件中转（拍照是低频操作，但写盘+读盘会阻塞主线程数十毫秒）
    sf::Image frame = tex.texture.copyToImage();

    // 沿用 XImage 的纹理创建流程：先同步尺寸，再把像素上传到目标纹理
    // 注意用构造函数而非 resize——SFML 3 中 resize 对未创建的 0x0 纹理无效
    sf::Vector2u size = frame.getSize();
    img.w = (int)size.x;
    img.h = (int)size.y;
    img.texture = sf::Texture(size);
    img.texture.update(frame);
}

void CameraManager::Rote()
{
    if (Rotation.end - 90 <= -360)
    {
        Rotation.value += 360;
        Rotation.end += 360;
    }
    Rotation.SetAnimation(Rotation.end - 90);
}

//自动拍照
#pragma region MyRegion

/// 帧差检测器（自维护上一帧，按纹理尺寸缓存）
class FrameDiffChecker
{
public:
    /// @param cur       当前帧纹理
    /// @param threshold 差异阈值 [0,1]，如 0.005 = 0.5%
    /// @param stable    连续多少帧差异低于阈值才返回 true（防抖）
    bool operator()(const sf::Texture& cur, float threshold = 0.3f, int stable = 1)
    {
        const sf::Vector2u size = cur.getSize();
        const uint64_t key = (uint64_t(size.x) << 32) | size.y;

        auto& slot = m_cache[key];

        static bool init = false;
        // 首帧 / 尺寸变化 → 初始化，直接返回 false
        if (!init)
        {
            slot.prevTex = cur;
            slot.stableCount = 0;
            init = true;
            return false;
        }

        // ---- GPU 降采样帧差 ----
        const unsigned downW = max(size.x / 4, 1u);
        const unsigned downH = max(size.y / 4, 1u);

        if (slot.rt.getSize() != sf::Vector2u(downW, downH))
            slot.rt.resize({ downW, downH });

        // 首次加载着色器
        static bool shaderReady = false;
        if (!shaderReady)
        {
            static const char* src = R"(
                uniform sampler2D uCur;
                uniform sampler2D uPrev;
                uniform vec2 uTexelSize;
                void main()
                {
                    vec2 uv = gl_FragCoord.xy * uTexelSize;
                    vec3 c1 = texture2D(uCur, uv).rgb;
                    vec3 c2 = texture2D(uPrev, uv).rgb;
                    vec3 diff = abs(c1 - c2);
                    float luma = dot(diff, vec3(0.299, 0.587, 0.114));
                    gl_FragColor = vec4(vec3(luma), 1.0);
                }
            )";
            s_shader.loadFromMemory(src, sf::Shader::Type::Fragment);
            shaderReady = true;
        }

        s_shader.setUniform("uCur", cur);
        s_shader.setUniform("uPrev", slot.prevTex);
        s_shader.setUniform("uTexelSize", sf::Glsl::Vec2(
            1.0f / (float)downW, 1.0f / (float)downH));

        sf::Sprite spr(cur);
        slot.rt.clear(sf::Color::Black);
        slot.rt.draw(spr, &s_shader);
        slot.rt.display();

        // ---- CPU 回读（降采样后极小） ----
        const sf::Image img = slot.rt.getTexture().copyToImage();
        const uint8_t* p = img.getPixelsPtr();
        const size_t total = downW * downH;

        size_t diffCount = 0;
        for (size_t i = 0; i < total * 4; i += 4)
        {
            float luma = p[i] / 255.0f;

            // 相对阈值：亮部允许更大误差
            float dynamicThresh = 0.015f + luma * 0.03f;

            if (luma > dynamicThresh)
                ++diffCount;
        }

        const float ratio = (float)diffCount / (float)total;

        // ---- 更新状态 ----
        slot.prevTex = cur; // 滚动上一帧

        if (ratio < threshold)
        {
            ++slot.stableCount;
            return slot.stableCount >= stable;
        }
        else
        {
            slot.stableCount = 0;
            return false;
        }
    }

private:
    struct Slot
    {
        sf::RenderTexture rt;
        sf::Texture prevTex; // 自维护上一帧
        int stableCount = 0;
    };

    std::unordered_map<uint64_t, Slot> m_cache;
    static inline sf::Shader s_shader; // 全局复用一份
};
FrameDiffChecker FrameChecker;

bool CameraManager::NeedAutoPhoto()
{
    static int PhotoClock = 0;

    if (!WriteCamera::EnableAutoPhoto)
    {
        PhotoClock = 0;
        return false;
    }

    // FrameChecker 返回 true 表示"画面稳定"
    if (!FrameChecker(tex.texture))
    {
        PhotoClock = 0;   // 画面在动 → 重新计时
        return false;
    }

    ++PhotoClock;

    if (PhotoClock > 40)
    {
        PhotoClock = 0;   // 先复位，再返回，否则成为死代码
        return true;      // 稳定超过 120 帧 → 拍一张
    }

    return false;
}


#pragma endregion