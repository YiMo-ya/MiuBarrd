#pragma once

// ShareCamera
// 手机相机投屏到 PC（手机 -> PC）。
//
// 访问方式（仅局域网直连）：
//   手机与 PC 处于同一局域网时，访问 http://<本机IP>:<port>/

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <sys/select.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <fcntl.h>
#  define SOCKET       int
#  define INVALID_SOCKET (-1)
#  define SOCKET_ERROR  (-1)
#  define closesocket   ::close
#endif

#include "Xs/Xs.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <vector>

struct CameraFrame
{
    std::vector<uint8_t> jpeg;
    uint32_t             seq = 0;
    bool                 valid = false;
};

class ShareCamera
{
public:
    ~ShareCamera();

    // ---- 生命周期 ----

    /// <summary>启动投屏服务：构建局域网 URL 并开启 HTTP 服务器。</summary>
    /// <returns>服务器启动成功返回 true；端口绑定失败返回 false。</returns>
    bool start();

    /// <summary>停止投屏服务：关闭服务器并清空拍照/上传队列。</summary>
    void stop();

    /// <summary>查询服务是否正在运行。</summary>
    bool isRunning() const { return running_; }

    /// <summary>初始化网络环境（Windows 下执行 WSAStartup）。</summary>
    /// <param name="port">HTTP 服务器监听端口，默认 8080。</param>
    void Init(int port = 8080);

    // ---- 二维码 / URL ----

    /// <summary>生成局域网访问地址对应的二维码纹理。</summary>
    /// <param name="scale">每个二维码模块的像素边长。</param>
    /// <param name="border">二维码四周的空白模块数。</param>
    /// <returns>二维码纹理；地址为空时返回空纹理。</returns>
    sf::Texture getQRTexture(int scale = 8, int border = 4) const;

    /// <summary>获取局域网访问地址（形如 http://&lt;本机IP&gt;:&lt;port&gt;/）。</summary>
    std::wstring getURL() const;

    // ---- 投屏：手机 -> PC ----
    // 原有接口，行为不变
    bool updateTexture(sf::Texture& out);
    bool hasNewFrame() const;

    // <<< ASPECT: 新增——根据手机比例算好显示矩形，直接 draw
    // areaW/areaH = 你想让画面占据的最大区域（比如窗口的 80% 宽高）
    // 返回 sf::RectangleShape，已设好 size/position/texture
    sf::RectangleShape getDisplayRect(const sf::Texture& tex,
        float areaW, float areaH) const;

    // <<< ASPECT: 获取当前手机画面比例（宽/高），未收到时返回 0
    float getAspectRatio() const;

    // ---- 拍照检测 ----
    bool hasNewPhoto() const;
    CameraFrame popPhoto();

    // ---- 上传图片检测 ----
    bool hasUploadedImage() const;
    CameraFrame popUploadedImage();

    // ---- 控制 ----
    void requestStopStream();
    void requestExitCamera();

    static int findAvailablePort(int preferred = 8080);

private:
    bool startServer();
    void runServer();
    void buildURL();
    std::string getLocalIP() const;

    void handleClient(SOCKET client);
    void handlePage(SOCKET s);
    void handleStream(SOCKET s);
    void handleFrame(SOCKET s, const std::vector<uint8_t>& body);  // <<< ASPECT: 不再需要 raw
    void handleCapture(SOCKET s);
    void handleUpload(SOCKET s, const std::vector<uint8_t>& body, const std::string& contentType);
    void handleStop(SOCKET s);
    void handleExit(SOCKET s);

    static std::vector<uint8_t> readHttpRequest(SOCKET s);
    static std::string          parseHeader(const std::vector<uint8_t>& raw, const std::string& key);
    static size_t               parseContentLength(const std::vector<uint8_t>& raw);
    static std::vector<uint8_t> parseBody(const std::vector<uint8_t>& raw);

    void sendMJPEGHeader(SOCKET s);
    void sendJPEGFrame(SOCKET s, const std::vector<uint8_t>& jpeg);
    static CameraFrame parseMultipart(const std::vector<uint8_t>& body,
        const std::string& boundary);

    int          port_ = 8080;
    mutable std::mutex urlMutex_;
    std::wstring url_;

    SOCKET              serverSocket_ = INVALID_SOCKET;
    std::atomic<bool>   running_{ false };
    std::thread         serverThread_;

    // 最新投屏帧
    mutable std::mutex    frameMutex_;
    std::vector<uint8_t>  currentJPEG_;
    uint32_t              frameSeq_ = 0;
    uint32_t              consumedSeq_ = 0;

    // <<< ASPECT: 手机画面比例（宽/高），由 /aspect 路由写入
    mutable std::mutex    aspectMutex_;
    float                 currentAspect_ = 0.0f;

    // 拍照队列
    mutable std::mutex    photoMutex_;
    std::queue<CameraFrame> photoQueue_;

    // 上传队列
    mutable std::mutex       uploadMutex_;
    std::queue<CameraFrame>  uploadQueue_;

    std::atomic<bool>    stopRequested_{ false };
    std::atomic<bool>    exitRequested_{ false };

    // HTML
    std::wstring         htmlPath_;
    mutable std::mutex   htmlMutex_;
    std::string          cachedHTML_;
    bool                 htmlLoaded_ = false;

    std::string loadHTML() const;
    const std::string& getHTML();
    void reloadHTML();
};