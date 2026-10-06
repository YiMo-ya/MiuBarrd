// ShareCamera.cpp — 完整修复版
#include "ShareCamera.h"

#include "Xs/Xs.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "QR/qrcodegen.hpp"

#ifdef _WIN32
#  define SLEEP_MS(ms) Sleep(ms)
#else
#  include <chrono>
#  include <thread>
#  define SLEEP_MS(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#endif

namespace
{
    std::string httpDate()
    {
        char buf[64];
#ifdef _WIN32
        SYSTEMTIME st;
        GetLocalTime(&st);
        sprintf_s(buf, sizeof(buf), "%02d:%02d:%02d",
            st.wHour, st.wMinute, st.wSecond);
#else
        time_t t = time(nullptr);
        tm* tm = localtime(&t);
        strftime(buf, sizeof(buf), "%T", tm);
#endif
        return std::string(buf);
    }

    // <<< FIX: 全部 send 改为循环发送，确保数据完整
    void httpResponse(SOCKET s, int code, const std::string& codeText,
        const std::string& contentType,
        const std::vector<uint8_t>& body)
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << code << " " << codeText << "\r\n"
            << "Content-Type: " << contentType << "\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Cache-Control: no-store\r\n"
            << "Connection: close\r\n"
            << "\r\n";
        std::string head = oss.str();

        const char* p = head.data();
        int remaining = (int)head.size();
        while (remaining > 0)
        {
            int n = send(s, p, remaining, 0);
            if (n <= 0) return;
            p += n;
            remaining -= n;
        }

        if (!body.empty())
        {
            p = (const char*)body.data();
            remaining = (int)body.size();
            while (remaining > 0)
            {
                int n = send(s, p, remaining, 0);
                if (n <= 0) break;
                p += n;
                remaining -= n;
            }
        }
    }

    void httpResponseText(SOCKET s, const std::string& text)
    {
        httpResponse(s, 200, "OK", "text/plain; charset=utf-8",
            std::vector<uint8_t>(text.begin(), text.end()));
    }
} // namespace

// ================= HTML 文件读取 =================

std::string ShareCamera::loadHTML() const
{
    std::wstring tryPaths[] = {
        htmlPath_,
        L"Net/ShareCamera.html",
        L"./Net/ShareCamera.html",
        L"../Net/ShareCamera.html",
    };

    FILE* f = nullptr;
    std::wstring foundPath;

    for (const auto& p : tryPaths)
    {
        if (p.empty()) continue;
        f = _wfopen(p.c_str(), L"rb");
        if (f) { foundPath = p; break; }
    }

    if (!f)
    {
        wchar_t exePath[MAX_PATH] = { 0 };
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0)
        {
            std::wstring dir(exePath);
            size_t slash = dir.find_last_of(L"\\/");
            if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);
            std::wstring full = dir + L"Net/ShareCamera.html";
            f = _wfopen(full.c_str(), L"rb");
            if (f) foundPath = full;
        }
    }

    if (!f)
    {
        std::cerr << "[ShareCamera] ERROR: Net/ShareCamera.html not found!" << std::endl;
        return "<!DOCTYPE html><html><body>"
            "<h1 style='color:red;font-family:sans-serif'>"
            "Error: Net/ShareCamera.html not found!"
            "</h1></body></html>";
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0)
    {
        fclose(f);
        return "<h1>Empty HTML file</h1>";
    }

    std::string content(static_cast<size_t>(size), '\0');
    fread(&content[0], 1, static_cast<size_t>(size), f);
    fclose(f);

    std::cout << "[ShareCamera] Loaded HTML: "
        << xs::XString::Convert::wstring_to_utf8(foundPath)
        << " (" << size << " bytes)" << std::endl;

    return content;
}

const std::string& ShareCamera::getHTML()
{
    std::lock_guard<std::mutex> lock(htmlMutex_);
    if (!htmlLoaded_)
    {
        cachedHTML_ = loadHTML();
        htmlLoaded_ = true;
    }
    return cachedHTML_;
}

void ShareCamera::reloadHTML()
{
    std::lock_guard<std::mutex> lock(htmlMutex_);
    cachedHTML_ = loadHTML();
    htmlLoaded_ = true;
    std::cout << "[ShareCamera] HTML reloaded" << std::endl;
}

// ================= 构造 / 析构 =================

void ShareCamera::Init(int port)
{
    port_ = port;

#ifdef _WIN32
    WSADATA wsa;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (result != 0)
    {
        std::cerr << "[ShareCamera] WSAStartup failed, err=" << result << std::endl;
        return;
    }
    std::cout << "[ShareCamera] WSA initialized, port=" << port_ << std::endl;
#endif
}

ShareCamera::~ShareCamera()
{
    stop();          // 统一在 stop() 中关闭服务器并清空队列
#ifdef _WIN32
    WSACleanup();
#endif
}

// ================= 生命周期 =================

bool ShareCamera::start()
{
    if (running_) stop();

    buildURL();
    bool ok = startServer();

    if (ok)
        std::cout << "[ShareCamera] started (LAN mode)" << std::endl;

    return ok;
}

void ShareCamera::stop()
{
    if (!running_) return;

    running_ = false;
    stopRequested_ = true;
    exitRequested_ = true;

    if (serverSocket_ != INVALID_SOCKET)
    {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }
    if (serverThread_.joinable())
        serverThread_.join();

    {
        std::lock_guard<std::mutex> lk(photoMutex_);
        while (!photoQueue_.empty()) photoQueue_.pop();
    }
    {
        std::lock_guard<std::mutex> lk(uploadMutex_);
        while (!uploadQueue_.empty()) uploadQueue_.pop();
    }
}

// ================= URL / QR =================

std::string ShareCamera::getLocalIP() const
{
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return "127.0.0.1";
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &target.sin_addr);
    connect(s, (sockaddr*)&target, sizeof(target));
    sockaddr_in local{};
    int len = sizeof(local);
    getsockname(s, (sockaddr*)&local, &len);
    char ip[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
    closesocket(s);
    return std::string(ip);
#else
    return "127.0.0.1";
#endif
}

void ShareCamera::buildURL()
{
    std::lock_guard<std::mutex> lk(urlMutex_);
    // 仅局域网直连：用本机内网 IP 拼出手机可访问的地址
    std::string ip = getLocalIP();
    url_ = L"http://" + xs::XString::Convert::utf8_to_wstring(ip)
        + L":" + std::to_wstring(port_) + L"/";
}

std::wstring ShareCamera::getURL() const
{
    std::lock_guard<std::mutex> lk(urlMutex_);
    return url_;
}

sf::Texture ShareCamera::getQRTexture(int scale, int border) const
{
    std::wstring target = getURL();
    if (target.empty()) return sf::Texture();
    std::string u8 = xs::XString::Convert::wstring_to_utf8(target);
    qrcodegen::QrCode code =
        qrcodegen::QrCode::encodeText(u8.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    int modules = code.getSize();
    int dim = modules + border * 2;
    int px = dim * scale;
    sf::Image img(sf::Vector2u((unsigned)px, (unsigned)px), sf::Color::White);

    for (int my = -border; my < modules + border; ++my)
    {
        for (int mx = -border; mx < modules + border; ++mx)
        {
            if (mx < 0 || mx >= modules || my < 0 || my >= modules) continue;
            if (!code.getModule(mx, my)) continue;
            int pxX = (mx + border) * scale;
            int pxY = (my + border) * scale;
            for (int dy = 0; dy < scale; ++dy)
                for (int dx = 0; dx < scale; ++dx)
                    img.setPixel(sf::Vector2u((unsigned)(pxX + dx),
                        (unsigned)(pxY + dy)),
                        sf::Color::Black);
        }
    }
    sf::Texture tex;
    tex.loadFromImage(img);
    tex.setSmooth(false);
    return tex;
}

int ShareCamera::findAvailablePort(int preferred)
{
    for (int p = preferred; p < preferred + 100; ++p)
    {
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCKET) continue;
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = INADDR_ANY;
        a.sin_port = htons(p);
        if (bind(s, (sockaddr*)&a, sizeof(a)) == 0)
        {
            closesocket(s);
            return p;
        }
        closesocket(s);
    }
    return preferred;
}

// ================= 服务器 =================

bool ShareCamera::startServer()
{
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == INVALID_SOCKET) return false;

    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR,
        (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(serverSocket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        int err = 0;
#ifdef _WIN32
        err = WSAGetLastError();
#endif
        std::cerr << "[ShareCamera] bind failed on port " << port_
            << " err=" << err << std::endl;
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }
    if (listen(serverSocket_, 8) == SOCKET_ERROR)
    {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }
    running_ = true;
    serverThread_ = std::thread([this]() { runServer(); });
    std::cout << "[ShareCamera] listening on http://" << getLocalIP()
        << ":" << port_ << "/" << std::endl;
    return true;
}

void ShareCamera::runServer()
{
    while (running_)
    {
        sockaddr_in client{};
        socklen_t len = sizeof(client);
        SOCKET cs = accept(serverSocket_, (sockaddr*)&client, &len);
        if (cs == INVALID_SOCKET)
        {
            if (!running_) break;
            continue;
        }
        char ip[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(client.sin_family, &client.sin_addr, ip, sizeof(ip));
        std::cout << "[ShareCamera] client: " << ip << std::endl;

        std::thread([this, cs]() { handleClient(cs); }).detach();
    }
    std::cout << "[ShareCamera] server stopped" << std::endl;
}

// ================= HTTP 请求分发 =================

void ShareCamera::handleClient(SOCKET s)
{
    auto raw = readHttpRequest(s);
    if (raw.empty()) { closesocket(s); return; }

    std::string reqLine;
    size_t eol = raw.size();
    for (size_t i = 0; i + 1 < raw.size(); ++i)
    {
        if (raw[i] == '\r' && raw[i + 1] == '\n') { eol = i; break; }
    }
    reqLine = std::string((const char*)raw.data(), eol);
    std::string method, path;
    {
        std::istringstream iss(reqLine);
        iss >> method >> path;
    }

    std::string clenStr = parseHeader(raw, "Content-Length");
    size_t clen = clenStr.empty() ? 0 : (size_t)std::atoll(clenStr.c_str());
    std::string ctype = parseHeader(raw, "Content-Type");
    std::vector<uint8_t> body = parseBody(raw);

    while (body.size() < clen && running_)
    {
        char tmp[4096];
        int n = recv(s, tmp, sizeof(tmp), 0);
        if (n <= 0) break;
        body.insert(body.end(), tmp, tmp + n);
    }

    std::cout << "[ShareCamera] " << method << " " << path
        << " body=" << body.size() << std::endl;

    if (path == "/" || path == "/index.html")
        handlePage(s);
    else if (path == "/stream")
        handleStream(s);
    else if (path == "/frame")
        handleFrame(s, body);
    else if (path == "/capture")
        handleCapture(s);
    else if (path == "/upload")
        handleUpload(s, body, ctype);
    else if (path == "/stop")
        handleStop(s);
    else if (path == "/exit")
        handleExit(s);
    else if (path == "/admin/reload")
    {
        reloadHTML();
        httpResponseText(s, "reloaded");
    }
    else
        httpResponse(s, 404, "Not Found", "text/plain", { (uint8_t)'?', 1 });

    closesocket(s);
}

void ShareCamera::handlePage(SOCKET s)
{
    const std::string& html = getHTML();
    std::cout << "[ShareCamera] serving page, size=" << html.size() << std::endl;  // <<< FIX: 诊断
    httpResponse(s, 200, "OK", "text/html; charset=utf-8",
        std::vector<uint8_t>(html.begin(), html.end()));
}

// <<< FIX: sendMJPEGHeader 改为循环 send
void ShareCamera::sendMJPEGHeader(SOCKET s)
{
    const char* head =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=--frame\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n";
    const char* p = head;
    int remaining = (int)std::strlen(head);
    while (remaining > 0)
    {
        int n = send(s, p, remaining, 0);
        if (n <= 0) return;
        p += n;
        remaining -= n;
    }
}

// <<< FIX: sendJPEGFrame 改为循环 send
void ShareCamera::sendJPEGFrame(SOCKET s, const std::vector<uint8_t>& jpeg)
{
    std::ostringstream oss;
    oss << "--frame\r\n"
        << "Content-Type: image/jpeg\r\n"
        << "Content-Length: " << jpeg.size() << "\r\n"
        << "\r\n";
    std::string h = oss.str();

    const char* p = h.data();
    int remaining = (int)h.size();
    while (remaining > 0)
    {
        int n = send(s, p, remaining, 0);
        if (n <= 0) return;
        p += n;
        remaining -= n;
    }

    p = (const char*)jpeg.data();
    remaining = (int)jpeg.size();
    while (remaining > 0)
    {
        int n = send(s, p, remaining, 0);
        if (n <= 0) break;
        p += n;
        remaining -= n;
    }

    const char* crlf = "\r\n";
    send(s, crlf, 2, 0);
}

void ShareCamera::handleStream(SOCKET s)
{
    sendMJPEGHeader(s);
    uint32_t lastSeq = 0;
    int skipCount = 0;
    while (running_ && !stopRequested_)
    {
        std::vector<uint8_t> jpeg;
        uint32_t seq = 0;
        {
            std::lock_guard<std::mutex> lk(frameMutex_);
            jpeg = currentJPEG_;
            seq = frameSeq_;
        }
        if (!jpeg.empty() && seq != lastSeq)
        {
            sendJPEGFrame(s, jpeg);
            lastSeq = seq;
            skipCount = 0;
        }
        else
        {
            skipCount++;
            if (skipCount > 300)  // ~10秒无帧，断开
            {
                std::cout << "[ShareCamera] stream timeout (no frames)" << std::endl;
                break;
            }
        }
        SLEEP_MS(33);
    }
}

void ShareCamera::handleFrame(SOCKET s, const std::vector<uint8_t>& body)
{
    if (!body.empty())
    {
        std::lock_guard<std::mutex> lk(frameMutex_);
        currentJPEG_ = body;
        frameSeq_++;
    }
    httpResponseText(s, "ok");
}

void ShareCamera::handleCapture(SOCKET s)
{
    std::vector<uint8_t> jpeg;
    {
        std::lock_guard<std::mutex> lk(frameMutex_);
        jpeg = currentJPEG_;
    }
    if (!jpeg.empty())
    {
        CameraFrame f;
        f.jpeg = jpeg;
        f.seq = ++frameSeq_;
        f.valid = true;
        std::lock_guard<std::mutex> lk(photoMutex_);
        photoQueue_.push(f);
        std::cout << "[ShareCamera] capture queued (photos=" << photoQueue_.size() << ")"
            << std::endl;
    }
    httpResponseText(s, "captured");
}

void ShareCamera::handleUpload(SOCKET s, const std::vector<uint8_t>& body,
    const std::string& contentType)
{
    CameraFrame f;
    if (body.empty())
    {
        httpResponse(s, 400, "Bad Request", "text/plain", {});
        return;
    }
    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        std::string bound = "boundary=";
        auto pos = contentType.find(bound);
        std::string boundary;
        if (pos != std::string::npos)
        {
            boundary = contentType.substr(pos + bound.size());
            boundary.erase(std::remove(boundary.begin(), boundary.end(), '"'), boundary.end());
        }
        f = parseMultipart(body, boundary);
    }
    else
    {
        f.jpeg = body;
        f.seq = ++frameSeq_;
        f.valid = true;
    }

    if (f.valid)
    {
        std::lock_guard<std::mutex> lk(uploadMutex_);
        uploadQueue_.push(f);
        std::cout << "[ShareCamera] upload queued (images=" << uploadQueue_.size() << ")"
            << std::endl;
    }
    httpResponseText(s, "uploaded");
}

void ShareCamera::handleStop(SOCKET s)
{
    stopRequested_ = true;
    httpResponseText(s, "stopped");
}

void ShareCamera::handleExit(SOCKET s)
{
    exitRequested_ = true;
    stopRequested_ = true;
    httpResponseText(s, "bye");
}

// ================= PC 端取数据 =================

bool ShareCamera::hasNewFrame() const
{
    std::lock_guard<std::mutex> lk(frameMutex_);
    return frameSeq_ != consumedSeq_;
}

bool ShareCamera::updateTexture(sf::Texture& out)
{
    std::vector<uint8_t> jpeg;
    uint32_t seq = 0;
    {
        std::lock_guard<std::mutex> lk(frameMutex_);
        jpeg = currentJPEG_;
        seq = frameSeq_;
    }
    if (jpeg.empty()) return false;

    sf::Image img;
    if (!img.loadFromMemory(jpeg.data(), jpeg.size()))
        return false;

    if (out.getSize() != img.getSize())
    {
        if (!out.resize(img.getSize())) return false;
    }
    out.update(img);
    consumedSeq_ = seq;
    return true;
}

bool ShareCamera::hasNewPhoto() const
{
    std::lock_guard<std::mutex> lk(photoMutex_);
    return !photoQueue_.empty();
}

CameraFrame ShareCamera::popPhoto()
{
    std::lock_guard<std::mutex> lk(photoMutex_);
    if (photoQueue_.empty()) return CameraFrame();
    CameraFrame f = photoQueue_.front();
    photoQueue_.pop();
    return f;
}

bool ShareCamera::hasUploadedImage() const
{
    std::lock_guard<std::mutex> lk(uploadMutex_);
    return !uploadQueue_.empty();
}

CameraFrame ShareCamera::popUploadedImage()
{
    std::lock_guard<std::mutex> lk(uploadMutex_);
    if (uploadQueue_.empty()) return CameraFrame();
    CameraFrame f = uploadQueue_.front();
    uploadQueue_.pop();
    return f;
}

void ShareCamera::requestStopStream() { stopRequested_ = true; }
void ShareCamera::requestExitCamera() { exitRequested_ = true; stopRequested_ = true; }

// ================= ASPECT: 显示适配 =================

float ShareCamera::getAspectRatio() const
{
    std::lock_guard<std::mutex> lk(aspectMutex_);
    return currentAspect_;
}

sf::RectangleShape ShareCamera::getDisplayRect(const sf::Texture& tex,
    float areaW, float areaH) const
{
    sf::RectangleShape rect;

    // 优先用手机上报的画面比例；未收到时退化为纹理自身尺寸比例
    float aspect = getAspectRatio();
    if (aspect <= 0.0f)
    {
        sf::Vector2u sz = tex.getSize();
        if (sz.y == 0) return rect;
        aspect = (float)sz.x / (float)sz.y;
    }

    // 在 areaW x areaH 区域内按比例做「contain」缩放，保持画面不变形
    float w = areaW;
    float h = areaW / aspect;
    if (h > areaH)
    {
        h = areaH;
        w = areaH * aspect;
    }

    rect.setSize(sf::Vector2f(w, h));
    rect.setTexture(&tex);
    rect.setPosition(sf::Vector2f((areaW - w) * 0.5f, (areaH - h) * 0.5f));
    return rect;
}

// ================= HTTP 解析工具 =================

std::vector<uint8_t> ShareCamera::readHttpRequest(SOCKET s)
{
    std::vector<uint8_t> buf;
    buf.reserve(65536);
    char tmp[4096];
    for (;;)
    {
        int n = recv(s, tmp, sizeof(tmp), 0);
        if (n <= 0) break;
        buf.insert(buf.end(), tmp, tmp + n);
        for (size_t i = 0; i + 3 < buf.size(); ++i)
        {
            if (buf[i] == '\r' && buf[i + 1] == '\n' &&
                buf[i + 2] == '\r' && buf[i + 3] == '\n')
                return buf;
        }
        if (buf.size() > 8 * 1024 * 1024) break;
    }
    return buf;
}

std::string ShareCamera::parseHeader(const std::vector<uint8_t>& raw, const std::string& key)
{
    std::string hay((const char*)raw.data(), raw.size());
    std::string keyL = key, hayL = hay;
    std::transform(keyL.begin(), keyL.end(), keyL.begin(), ::tolower);
    std::transform(hayL.begin(), hayL.end(), hayL.begin(), ::tolower);

    size_t headEnd = hay.find("\r\n\r\n");
    if (headEnd == std::string::npos) headEnd = hay.size();

    size_t p = hayL.find(keyL + ":");
    if (p == std::string::npos || p >= headEnd) return "";
    p += keyL.size() + 1;
    size_t e = hay.find("\r\n", p);
    if (e == std::string::npos || e > headEnd) e = headEnd;
    std::string val = hay.substr(p, e - p);
    val.erase(0, val.find_first_not_of(" \t"));
    val.erase(val.find_last_not_of(" \t\r\n") + 1);
    return val;
}

size_t ShareCamera::parseContentLength(const std::vector<uint8_t>& raw)
{
    std::string v = parseHeader(raw, "Content-Length");
    return v.empty() ? 0 : (size_t)std::atoll(v.c_str());
}

std::vector<uint8_t> ShareCamera::parseBody(const std::vector<uint8_t>& raw)
{
    auto it = std::search(raw.begin(), raw.end(),
        std::vector<uint8_t>{'\r', '\n', '\r', '\n'}.begin(),
        std::vector<uint8_t>{'\r', '\n', '\r', '\n'}.end());
    if (it == raw.end()) return {};
    return std::vector<uint8_t>(it + 4, raw.end());
}

// ================= multipart/form-data 解析 =================

CameraFrame ShareCamera::parseMultipart(const std::vector<uint8_t>& body,
    const std::string& boundary)
{
    CameraFrame f;
    if (boundary.empty() || body.empty()) return f;

    std::string b = "--" + boundary;
    std::vector<uint8_t> delim(b.begin(), b.end());

    auto pos = std::search(body.begin(), body.end(), delim.begin(), delim.end());
    if (pos == body.end()) return f;

    auto next = std::search(pos + (ptrdiff_t)delim.size(), body.end(),
        delim.begin(), delim.end());
    if (next == body.end()) next = body.end();

    auto hEnd = std::search(pos, next,
        std::vector<uint8_t>{'\r', '\n', '\r', '\n'}.begin(),
        std::vector<uint8_t>{'\r', '\n', '\r', '\n'}.end());
    if (hEnd == next) return f;

    std::string header((const char*)&*pos, hEnd - pos);
    std::string headerL = header;
    std::transform(headerL.begin(), headerL.end(), headerL.begin(), ::tolower);

    bool isImage = headerL.find("content-type: image/") != std::string::npos ||
        headerL.find("filename=") != std::string::npos;

    if (!isImage) return f;

    auto contentBegin = hEnd + 4;
    auto contentEnd = next;
    if (contentEnd - contentBegin >= 2 &&
        *(contentEnd - 2) == '\r' && *(contentEnd - 1) == '\n')
        contentEnd -= 2;

    f.jpeg.assign(contentBegin, contentEnd);
    f.seq = 0;
    f.valid = true;
    return f;
}