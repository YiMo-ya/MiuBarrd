#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "Xs/Xs.h"
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
#include <random>
#include <cstdio>

// qrcodegen
#include "QR/qrcodegen.hpp"

class ShareFile
{
public:
    ShareFile(int port = 8080);
    ~ShareFile();

    // ---- 局域网共享 ----
    bool start(const std::wstring& filePath);
    void stop();
    sf::Texture getQRTexture(int scale = 8, int border = 4) const;
    std::wstring getURL() const { return shareURL_; }
    bool isRunning() const { return running_; }

    // ---- 公网穿透（cloudflared）----
    bool startTunnel();
    void stopTunnel();
    bool isTunnelRunning() const { return tunnelRunning_; }
    std::wstring getPublicURL() const { return publicURL_; }
    sf::Texture getPublicQRTexture(int scale = 8, int border = 4) const;

    static int findAvailablePort(int preferred = 8080);

private:
    bool startServer();
    void runServer();
    std::string generateFilename() const;
    std::string getLocalIP() const;
    void buildURL();
    std::string readTunnelOutput();

    int port_;
    std::wstring filePath_;
    std::wstring shareURL_;

    // 局域网
    std::thread serverThread_;
    std::atomic<bool> running_;
    SOCKET serverSocket_;

    // 公网隧道
    bool tunnelRunning_ = false;
    std::wstring publicURL_;
    PROCESS_INFORMATION tunnelProc_ = {};
    HANDLE tunnelStdoutRead_ = nullptr;
    HANDLE tunnelStdoutWrite_ = nullptr;

    std::wstring fileName_;
};