#include "ShareFile.h"
#include "Xs/Xs.h"
#include <iostream>

// ============ 构造 / 析构 ============

ShareFile::ShareFile(int port)
    : port_(port), running_(false), serverSocket_(INVALID_SOCKET)
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    ZeroMemory(&tunnelProc_, sizeof(tunnelProc_));
}

ShareFile::~ShareFile()
{
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

// ============ 局域网共享 ============

bool ShareFile::start(const std::wstring& filePath)
{
    filePath_ = filePath;
    return startServer();
}

void ShareFile::stop()
{
    // 停隧道
    if (tunnelRunning_)
        stopTunnel();

    if (!running_) return;
    running_ = false;

    if (serverSocket_ != INVALID_SOCKET)
    {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }

    if (serverThread_.joinable())
        serverThread_.join();
}

sf::Texture ShareFile::getQRTexture(int scale, int border) const
{
    if (shareURL_.empty()) return {};

    std::string urlUtf8 = xs::XString::Convert::wstring_to_utf8(shareURL_);

    const qrcodegen::QrCode code =
        qrcodegen::QrCode::encodeText(urlUtf8.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);

    const int modules = code.getSize();
    const int dim = modules + border * 2;
    const int px = dim * scale;

    sf::Image img({ static_cast<unsigned>(px), static_cast<unsigned>(px) }, sf::Color::White);

    for (int my = -border; my < modules + border; ++my)
    {
        for (int mx = -border; mx < modules + border; ++mx)
        {
            if (mx < 0 || mx >= modules || my < 0 || my >= modules)
                continue;
            if (!code.getModule(mx, my))
                continue;

            const int pxX = (mx + border) * scale;
            const int pxY = (my + border) * scale;

            for (int dy = 0; dy < scale; ++dy)
                for (int dx = 0; dx < scale; ++dx)
                    img.setPixel({ static_cast<unsigned>(pxX + dx),
                        static_cast<unsigned>(pxY + dy) },
                        sf::Color::Black);
        }
    }

    sf::Texture tex;
    tex.loadFromImage(img);
    tex.setSmooth(false);
    return tex;
}

// ============ 内部实现（局域网） ============

bool ShareFile::startServer()
{
    if (running_) stop();

    buildURL();

#ifdef _WIN32
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == INVALID_SOCKET)
    {
        std::cerr << "[ShareFile] socket() failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(serverSocket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        std::cerr << "[ShareFile] bind() failed on port " << port_
            << " error=" << err << std::endl;
        if (err == 10048)
            std::cerr << "  ↑ 端口被占用！" << std::endl;
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }

    if (listen(serverSocket_, 5) == SOCKET_ERROR)
    {
        std::cerr << "[ShareFile] listen() failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }

    std::cout << "[ShareFile] Server listening on port " << port_ << std::endl;
#endif

    running_ = true;
    serverThread_ = std::thread([this]() { runServer(); });
    return true;
}

void ShareFile::runServer()
{
    std::cout << "[ShareFile] runServer thread started" << std::endl;

    while (running_)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket_, &readfds);

        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int selResult = select(0, &readfds, nullptr, nullptr, &tv);
        if (selResult == SOCKET_ERROR)
        {
            std::cerr << "[ShareFile] select() error: " << WSAGetLastError() << std::endl;
            break;
        }
        if (selResult == 0)
            continue;

        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSocket_, (sockaddr*)&clientAddr, &addrLen);

        if (clientSock == INVALID_SOCKET)
        {
            if (running_)
                std::cerr << "[ShareFile] accept() failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        char buf[4096] = {};
        int recvLen = recv(clientSock, buf, sizeof(buf) - 1, 0);
        if (recvLen <= 0)
        {
            closesocket(clientSock);
            continue;
        }

        buf[recvLen] = '\0';

        std::string request(buf);
        size_t getPos = request.find("GET ");
        if (getPos != std::string::npos)
        {
            size_t start = getPos + 4;
            size_t end = request.find(" ", start);
            if (end != std::string::npos)
            {
                std::string path = request.substr(start, end - start);
                std::cout << "[ShareFile] Request: " << path << std::endl;
            }
        }

        // 服务文件
        FILE* f = _wfopen(filePath_.c_str(), L"rb");
        if (!f)
        {
            std::cerr << "[ShareFile] Cannot open file" << std::endl;
            const char* notFound =
                "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\nConnection: close\r\n\r\nNot Found";
            send(clientSock, notFound, (int)strlen(notFound), 0);
            closesocket(clientSock);

            continue;
        }

        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        std::vector<char> fileData(static_cast<size_t>(fileSize));
        fread(fileData.data(), 1, static_cast<size_t>(fileSize), f);
        fclose(f);

        std::ostringstream header;
        header << "HTTP/1.1 200 OK\r\n";
        header << "Content-Type: image/png\r\n";
        header << "Content-Length: " << fileSize << "\r\n";
        header << "Cache-Control: no-store, no-cache\r\n";
        header << "Connection: close\r\n\r\n";

        std::string headerStr = header.str();
        send(clientSock, headerStr.c_str(), (int)headerStr.size(), 0);
        send(clientSock, fileData.data(), (int)fileData.size(), 0);
        closesocket(clientSock);
    }

    std::cout << "[ShareFile] runServer exiting" << std::endl;

    if (serverSocket_ != INVALID_SOCKET)
    {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }

    running_ = false;
}

std::string ShareFile::getLocalIP() const
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return "127.0.0.1";

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &target.sin_addr);
    connect(s, (sockaddr*)&target, sizeof(target));

    sockaddr_in localAddr{};
    int len = sizeof(localAddr);
    getsockname(s, (sockaddr*)&localAddr, &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &localAddr.sin_addr, ip, sizeof(ip));
    closesocket(s);
    return std::string(ip);
}

void ShareFile::buildURL()
{
    std::string ip = getLocalIP();
    size_t slash = filePath_.find_last_of(L"/\\");
    std::wstring fname = (slash != std::wstring::npos) ? filePath_.substr(slash + 1) : L"photo.png";
    shareURL_ = L"http://" + xs::XString::Convert::utf8_to_wstring(ip)
        + L":" + std::to_wstring(port_) + L"/" + fname;
}

std::string ShareFile::generateFilename() const
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    const char* hex = "0123456789abcdef";
    std::string s = "img_";
    for (int i = 0; i < 8; ++i)
        s += hex[dis(gen)];
    return s;
}

int ShareFile::findAvailablePort(int preferred)
{
    for (int p = preferred; p < preferred + 100; ++p)
    {
#ifdef _WIN32
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
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
#endif
    }
    return preferred;
}

// ============ 公网穿透（cloudflared） ============

bool ShareFile::startTunnel()
{
    if (tunnelRunning_) return true;

    // 找 cloudflared.exe
    wchar_t exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    std::wstring exePath(exeDir);
    size_t lastSlash = exePath.find_last_of(L"\\/");
    exePath = exePath.substr(0, lastSlash);
    std::wstring cfPath = exePath + L"\\Net\\cloudflared.exe";

    if (_waccess(cfPath.c_str(), 0) != 0)
    {
        std::cerr << "[ShareFile] cloudflared.exe not found at: "
            << xs::XString::Convert::wstring_to_utf8(cfPath) << std::endl;
        return false;
    }

    // 创建管道
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&tunnelStdoutRead_, &tunnelStdoutWrite_, &sa, 0))
    {
        std::cerr << "[ShareFile] CreatePipe failed" << std::endl;
        return false;
    }

    SetHandleInformation(tunnelStdoutRead_, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.hStdOutput = tunnelStdoutWrite_;
    si.hStdError = tunnelStdoutWrite_;
    si.dwFlags |= STARTF_USESTDHANDLES;

    std::wstring cmdline = L"\"" + cfPath + L"\" tunnel --url http://localhost:" + std::to_wstring(port_);

    ZeroMemory(&tunnelProc_, sizeof(tunnelProc_));
    if (!CreateProcessW(cfPath.c_str(), &cmdline[0], NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &tunnelProc_))
    {
        std::cerr << "[ShareFile] CreateProcess failed: " << GetLastError() << std::endl;
        CloseHandle(tunnelStdoutRead_);
        CloseHandle(tunnelStdoutWrite_);
        return false;
    }

    std::cout << "[ShareFile] cloudflared started (PID=" << tunnelProc_.dwProcessId << ")" << std::endl;

    // 等待 URL（最多 15 秒）
    publicURL_.clear();
    tunnelRunning_ = true;

    for (int i = 0; i < 30; ++i)
    {
        Sleep(500);

        std::string output = readTunnelOutput();
        if (!output.empty())
        {
            // ===== 只认 trycloudflare.com，不拼文件名 =====
            size_t pos = output.find("https://");
            while (pos != std::string::npos)
            {
                size_t end = output.find_first_of("\r\n \t\"'", pos);
                if (end == std::string::npos) end = output.size();

                std::string candidate = output.substr(pos, end - pos);

                // 只认真正的隧道域名
                if (candidate.find("trycloudflare.com") != std::string::npos)
                {
                    // 去掉末尾斜杠（如果有），再统一加一个
                    if (!candidate.empty() && candidate.back() == '/')
                        candidate.pop_back();

                    // 只拼根路径，不拼文件名（服务器忽略 path）
                    publicURL_ = xs::XString::Convert::utf8_to_wstring(candidate) + L"/";
                    std::wcout << L"[ShareFile] Public URL: " << publicURL_ << std::endl;
                    break;
                }

                pos = output.find("https://", end);
            }

            if (!publicURL_.empty())
                break;
        }

        DWORD exitCode;
        if (GetExitCodeProcess(tunnelProc_.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
        {
            std::cerr << "[ShareFile] cloudflared exited early, code=" << exitCode << std::endl;
            break;
        }
    }

    if (publicURL_.empty())
    {
        std::cerr << "[ShareFile] Failed to get tunnel URL (timeout)" << std::endl;
        stopTunnel();
        return false;
    }

    return true;
}

std::string ShareFile::readTunnelOutput()
{
    std::string result;
    char buf[1024];
    DWORD available = 0;

    PeekNamedPipe(tunnelStdoutRead_, NULL, 0, NULL, &available, NULL);

    if (available > 0)
    {
        DWORD bytesRead;
        while (available > 0 && result.size() < 8192)
        {
            DWORD toRead = min(available, (DWORD)sizeof(buf) - 1);
            if (ReadFile(tunnelStdoutRead_, buf, toRead, &bytesRead, NULL) && bytesRead > 0)
            {
                buf[bytesRead] = '\0';
                result += buf;
            }
            else
                break;
            PeekNamedPipe(tunnelStdoutRead_, NULL, 0, NULL, &available, NULL);
        }
    }

    if (!result.empty())
        std::cout << "[cf] " << result << std::endl;

    return result;
}

void ShareFile::stopTunnel()
{
    if (!tunnelRunning_) return;

    tunnelRunning_ = false;
    publicURL_.clear();

    if (tunnelProc_.hProcess)
    {
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, tunnelProc_.dwProcessId);
        Sleep(1000);

        DWORD exitCode;
        if (GetExitCodeProcess(tunnelProc_.hProcess, &exitCode) && exitCode == STILL_ACTIVE)
            TerminateProcess(tunnelProc_.hProcess, 1);

        CloseHandle(tunnelProc_.hProcess);
        CloseHandle(tunnelProc_.hThread);
        ZeroMemory(&tunnelProc_, sizeof(tunnelProc_));
    }

    if (tunnelStdoutRead_) { CloseHandle(tunnelStdoutRead_); tunnelStdoutRead_ = nullptr; }
    if (tunnelStdoutWrite_) { CloseHandle(tunnelStdoutWrite_); tunnelStdoutWrite_ = nullptr; }

    std::cout << "[ShareFile] Tunnel stopped" << std::endl;
}

sf::Texture ShareFile::getPublicQRTexture(int scale, int border) const
{
    if (publicURL_.empty()) return {};

    std::string urlUtf8 = xs::XString::Convert::wstring_to_utf8(publicURL_);

    const qrcodegen::QrCode code =
        qrcodegen::QrCode::encodeText(urlUtf8.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);

    const int modules = code.getSize();
    const int dim = modules + border * 2;
    const int px = dim * scale;

    sf::Image img({ static_cast<unsigned>(px), static_cast<unsigned>(px) }, sf::Color::White);

    for (int my = -border; my < modules + border; ++my)
    {
        for (int mx = -border; mx < modules + border; ++mx)
        {
            if (mx < 0 || mx >= modules || my < 0 || my >= modules)
                continue;
            if (!code.getModule(mx, my))
                continue;

            const int pxX = (mx + border) * scale;
            const int pxY = (my + border) * scale;

            for (int dy = 0; dy < scale; ++dy)
                for (int dx = 0; dx < scale; ++dx)
                    img.setPixel({ static_cast<unsigned>(pxX + dx),
                        static_cast<unsigned>(pxY + dy) },
                        sf::Color::Black);
        }
    }

    sf::Texture tex;
    tex.loadFromImage(img);
    tex.setSmooth(false);
    return tex;
}