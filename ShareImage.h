#pragma once

// ShareImage
// 手机相册图片上传到 PC（手机 -> PC），用于把手机里的图片写入板书页面。
//
// 与 ShareCamera 的区别：
//   ShareCamera 面向「实时投屏 / 拍照」，本类只处理「从相册选择图片上传」这一条链路，
//   上传成功的图片以「字节流 + 序号」形式放进队列，由 PC 端主线程取出后
//   逐张插入到 Write 的 PageData 中（一页一张图）。
//
// 访问方式（仅局域网直连）：
//   手机与 PC 处于同一局域网时，访问 http://<本机IP>:<port>/
//
// 内网穿透（cloudflared）：
//   手机与 PC 不在同一局域网时，可调用 startTunnel() 启动 cloudflared 快速隧道，
//   拿到 https://xxx.trycloudflare.com/ 公网地址后手机用移动网络也能直接上传。

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
#include <cstdint>

/// <summary>
/// 一张由手机上传到 PC 的图片数据。
/// </summary>
struct SharedImage
{
    /// 图片原始字节（JPEG / PNG 等，由手机端直接给出，PC 端自行解码）
    std::vector<uint8_t> data;

    /// 上传序号，单调递增，用于队列取出时区分多次上传
    uint32_t seq = 0;

    /// 是否为有效图片；false 表示解析失败或内容为空
    bool valid = false;

    /// 上传时的原始文件名（可能为空，浏览器未提供时）
    std::string name;

    /// 图片 MIME 类型，如 image/jpeg；无法判断时为空串
    /// 由客户端声明、文件名后缀、字节魔数三者依次推断，PC 端解码前可据此选择解码器
    std::string mimeType;
};

/// <summary>
/// 手机图片上传服务：提供一个极简 HTTP 服务器和网页，
/// 手机浏览器打开网页后从相册选图上传，PC 端轮询队列取图。
/// </summary>
class ShareImage
{
public:
    ~ShareImage();

    // ---- 生命周期 ----

    /// <summary>
    /// 初始化网络环境（Windows 下执行 WSAStartup）并记录监听端口。
    /// </summary>
    /// <param name="port">HTTP 服务器监听端口，默认 8080。</param>
    void Init(int port = 8080);

    /// <summary>
    /// 启动上传服务：构建局域网 URL 并开启 HTTP 服务器线程。
    /// </summary>
    /// <returns>服务器启动成功返回 true；端口绑定失败返回 false。</returns>
    bool start();

    /// <summary>
    /// 停止上传服务：关闭服务器套接字并清空待取队列。
    /// </summary>
    void stop();

    /// <summary>
    /// 查询服务是否正在运行。
    /// </summary>
    bool isRunning() const { return running_; }

    // ---- 公网穿透（cloudflared）----

    /// <summary>
    /// 启动 cloudflared 快速隧道，把本地 HTTP 服务暴露到公网。
    /// </summary>
    /// <returns>成功拿到公网地址返回 true；找不到 cloudflared.exe 或超时返回 false。</returns>
    bool startTunnel();

    /// <summary>
    /// 停止 cloudflared 隧道并清空公网地址。
    /// </summary>
    void stopTunnel();

    /// <summary>
    /// 查询公网隧道是否已建立（已拿到可用的公网地址）。
    /// </summary>
    bool isTunnelRunning() const { return tunnelRunning_; }

    /// <summary>
    /// 获取公网访问地址（形如 https://xxx.trycloudflare.com/）；未启动隧道时为空串。
    /// </summary>
    std::wstring getPublicURL() const;

    /// <summary>
    /// 生成公网访问地址对应的二维码纹理。
    /// </summary>
    /// <param name="scale">每个二维码模块的像素边长。</param>
    /// <param name="border">二维码四周的空白模块数。</param>
    /// <returns>二维码纹理；公网地址为空时返回空纹理。</returns>
    sf::Texture getPublicQRTexture(int scale = 8, int border = 4) const;

    // ---- 二维码 / URL ----

    /// <summary>
    /// 生成局域网访问地址对应的二维码纹理。
    /// </summary>
    /// <param name="scale">每个二维码模块的像素边长。</param>
    /// <param name="border">二维码四周的空白模块数。</param>
    /// <returns>二维码纹理；地址为空时返回空纹理。</returns>
    sf::Texture getQRTexture(int scale = 8, int border = 4) const;

    /// <summary>
    /// 获取局域网访问地址（形如 http://&lt;本机IP&gt;:&lt;port&gt;/）。
    /// </summary>
    std::wstring getURL() const;

    // ---- PC 端取图 ----

    /// <summary>
    /// 查询队列中是否还有待处理的图片。
    /// </summary>
    bool hasUploadedImage() const;

    /// <summary>
    /// 取出队列中最前面的一张图片；队列为空时返回 valid == false 的空对象。
    /// </summary>
    SharedImage popUploadedImage();

    /// <summary>
    /// 查询队列中待取图片数量。
    /// </summary>
    /// <returns>尚未被 popUploadedImage 取走的图片张数。</returns>
    size_t pendingCount() const;

    /// <summary>
    /// 取出当前所有待处理图片并清空队列（批量场景减少加锁次数）。
    /// </summary>
    /// <returns>按上传先后顺序排列的图片列表；队列为空时返回空列表。</returns>
    std::vector<SharedImage> popAllUploadedImages();

    /// <summary>
    /// 查询本次服务运行期间累计接收到的有效图片数量。
    /// 手机端页面据此显示「已传/总数」，与队列长度无关（取走图片后不会减少）。
    /// </summary>
    /// <returns>自 start() 以来成功入队的图片总张数。</returns>
    int getReceivedImageCount() const;

    /// <summary>
    /// 重置累计接收图片计数（开始新一轮上传会话时调用）。
    /// </summary>
    void resetReceivedImageCount();

    /// <summary>
    /// 请求停止网页端的轮询/上传行为（用户在 PC 端关闭共享时调用）。
    /// </summary>
    void requestStopStream();

    /// <summary>
    /// 请求网页端退出（用户手机侧确认关闭）。
    /// </summary>
    void requestExitCamera();

    /// <summary>
    /// 从指定端口开始查找一个空闲端口。
    /// </summary>
    /// <param name="preferred">起始端口号。</param>
    /// <returns>可绑定的端口号；全部被占用时返回 preferred。</returns>
    static int findAvailablePort(int preferred = 8080);

private:
    /// 创建监听套接字并启动服务线程。
    bool startServer();

    /// 服务主循环：accept 后为每个客户端开独立线程处理。
    void runServer();

    /// 构建局域网访问 URL。
    void buildURL();

    /// 读取 cloudflared 进程输出（非阻塞），并回显到控制台。
    std::string readTunnelOutput();

    /// 获取本机局域网 IP。
    std::string getLocalIP() const;

    /// 处理单个客户端连接：解析请求行并分发到具体处理函数。
    void handleClient(SOCKET client);

    /// 返回上传网页（HTML）。
    void handlePage(SOCKET s);

    /// 接收图片上传：解析 multipart 或裸 body，成功则入队。
    /// <param name="s">客户端套接字。</param>
    /// <param name="body">请求体原始字节。</param>
    /// <param name="contentType">请求头 Content-Type。</param>
    /// <param name="fileName">上传文件名（来自查询串 name 参数，可能为空）。</param>
    void handleUpload(SOCKET s, const std::vector<uint8_t>& body,
        const std::string& contentType,
        const std::string& fileName);

    /// 返回上传状态给网页端轮询。
    void handleStatus(SOCKET s);

    /// 以 JSON 形式返回待取图片概览（序号 / 大小 / 类型），便于手机端确认是否真正入队。
    void handlePending(SOCKET s);

    /// 网页端请求关闭上传。
    void handleStop(SOCKET s);

    /// 网页端请求退出。
    void handleExit(SOCKET s);

    /// 从套接字读取一次完整 HTTP 请求（含请求行、头部、已到达的 body）。
    static std::vector<uint8_t> readHttpRequest(SOCKET s);

    /// 从原始请求中解析指定头部字段。
    static std::string parseHeader(const std::vector<uint8_t>& raw, const std::string& key);

    /// 从原始请求中解析 body 部分。
    static std::vector<uint8_t> parseBody(const std::vector<uint8_t>& raw);

    /// 解析 multipart/form-data 请求体，取出第一个文件字段的内容。
    static SharedImage parseMultipart(const std::vector<uint8_t>& body,
        const std::string& boundary);

    /// 读取内置上传网页 HTML（首次访问时缓存）。
    std::string loadHTML() const;

    /// 获取缓存的上传网页 HTML。
    const std::string& getHTML();

    /// 丢弃缓存，重新从磁盘加载上传网页。
    void reloadHTML();

    int          port_ = 8080;                       /// 监听端口
    mutable std::mutex urlMutex_;                    /// 保护 url_
    std::wstring url_;                               /// 局域网访问地址

    std::atomic<bool>  tunnelRunning_{ false };      /// 公网隧道是否已建立
    mutable std::mutex publicUrlMutex_;              /// 保护 publicURL_
    std::wstring       publicURL_;                   /// 公网访问地址（cloudflared 分配）
    PROCESS_INFORMATION tunnelProc_{};               /// cloudflared 进程句柄
    HANDLE             tunnelStdoutRead_ = nullptr;  /// cloudflared 输出读取端
    HANDLE             tunnelStdoutWrite_ = nullptr; /// cloudflared 输出写入端

    SOCKET              serverSocket_ = INVALID_SOCKET; /// 监听套接字
    std::atomic<bool>   running_{ false };              /// 服务是否运行中
    std::thread         serverThread_;                  /// 服务线程

    mutable std::mutex       uploadMutex_;              /// 保护 uploadQueue_
    std::queue<SharedImage>  uploadQueue_;              /// 已上传待 PC 取走的图片

    std::atomic<uint32_t>    seqGen_{ 0 };              /// 上传序号发生器

    std::atomic<int>         receivedCount_{ 0 };       /// 本次运行累计接收的有效图片张数

    std::atomic<bool>    stopRequested_{ false };       /// 网页端请求停止
    std::atomic<bool>    exitRequested_{ false };       /// 网页端请求退出

    std::wstring         htmlPath_;                     /// 上传网页路径（可为空，走默认查找）
    mutable std::mutex   htmlMutex_;                    /// 保护 HTML 缓存
    std::string          cachedHTML_;                   /// 缓存的上传网页内容
    bool                 htmlLoaded_ = false;           /// 网页是否已加载过
};
