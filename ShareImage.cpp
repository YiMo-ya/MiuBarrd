#include "ShareImage.h"

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
    // ================= 内置上传页面 =================
    // 上一版「手机显示完成、电脑端没图」的根因在页面侧，因此这段 HTML 是本次改造的重点：
    //   1. 文件名通过查询串 ?name= 传给服务器，服务器据此推断 MIME / 扩展名；
    //   2. 改用 multipart/form-data 提交，避免部分手机浏览器把 File 当裸 body 发时
    //      被服务器误判为无 Content-Type 而丢弃；
    //   3. fetch 之后必须检查 res.ok 与 X-Image-Seq，只要服务器没真正入队就报错，
    //      不再出现「HTTP 层成功但队列为空」的假成功；
    //   4. 全部上传完再请求 /pending 二次确认，把电脑端真实队列数量回显给用户。
    // 中文一律用 UTF-8 字节转义，避免不同编译器源码字符集设置导致页面乱码。
    const char* kDefaultHTML =
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>\xE5\x90\x91MiuBarrd\xE4\xB8\x8A\xE4\xBC\xA0\xE5\x9B\xBE\xE7\x89\x87</title>"
        "<style>"
        "body{margin:0;padding:20px;background:#1e1e1e;color:#eee;"
        "font-family:system-ui,-apple-system,'PingFang SC',sans-serif;}"
        "h2{font-size:17px;font-weight:600;margin:0 0 14px;}"
        ".box{border:1px dashed #555;border-radius:12px;padding:20px;text-align:center;}"
        "input[type=file]{display:none;}"
        "label{display:inline-block;padding:12px 26px;border-radius:10px;"
        "background:#333;border:1px solid #666;cursor:pointer;font-size:15px;}"
        "label:active{background:#444;}"
        "#tip{margin-top:14px;font-size:14px;color:#9ad;min-height:22px;"
        "line-height:1.5;word-break:break-all;}"
        "#list{margin-top:14px;font-size:13px;color:#8a8;text-align:left;}"
        "</style></head><body>"
        "<h2>\xE5\x90\x91MiuBarrd\xE4\xB8\x8A\xE4\xBC\xA0\xE5\x9B\xBE\xE7\x89\x87</h2>"
        "<div class='box'>"
        "<input id='f' type='file' accept='image/*' multiple>"
        "<label for='f'>\xE9\x80\x89\xE6\x8B\xA9\xE5\x9B\xBE\xE7\x89\x87</label>"
        "<div id='tip'></div><div id='list'></div>"
        "</div>"
        "<script>"
        "var f=document.getElementById('f');"
        "var tip=document.getElementById('tip');"
        "var list=document.getElementById('list');"
        "function say(t,c){tip.style.color=c||'#9ad';tip.textContent=t;}"
        "function fmt(b){"
        "  if(b>=1048576)return (b/1048576).toFixed(1)+' MB/s';"
        "  return Math.max(0,Math.round(b/1024))+' KB/s';"
        "}"
        // onprogress 回调在部分手机浏览器上触发非常稀疏（甚至一次只触发一两回），
        // 因此这里只负责累计 sent/total 两个变量，真正刷新界面交给 500ms 定时器，
        // 保证进度与速度稳定地每 0.5 秒更新一次。
        // 计算方式：percent = 已发送字节 / 总字节 * 100。
        // 总字节优先用浏览器给的 e.total（含 multipart 开销，最准确），
        // 拿不到时退化为 file.size + 估算的 multipart 头部开销（MIME_PAD）。
        // 之所以不再只靠缓动：缓动会让「目标值已到 99」持续显示 99，
        // 用户看到的就是「一下就飙到 99%」，所以先保证分子分母本身合理。
        "var MIME_PAD=512;"
        "function send(file,done){" 
        "  var xhr=new XMLHttpRequest();"
        "  var t0=0,sent=0,timer=0,finished=0,shown=0;"
        // total 取浏览器上报的真实发送体长度；0 表示暂不可用
        "  var total=0;"
        "  function tick(){"
        "    var now=(new Date()).getTime();"
        "    if(!t0)t0=now;"
        // 分母优先真实总长，否则用文件大小加 multipart 头部估算
        "    var base=total>0?total:((file.size||0)+MIME_PAD);"
        "    var target=(base>0)?(sent/base*100):0;"
        "    var cap=finished?100:99;"
        "    if(target>cap)target=cap;"
        "    if(target<0)target=0;"
        // 进度只允许单调递增，避免回调抖动导致百分比回退
        "    if(shown<target){"
        "      shown+=(target-shown)*0.5;"
        "      if(target-shown<0.5)shown=target;"
        "    }"
        "    if(shown>cap)shown=cap;"
        "    var sec=(now-t0)/1000;"
        "    if(sec<0.1)sec=0.1;"
        // 速度后面追加「当前第几张/总数」，让用户知道整体进度而非单张进度
        "    say(shown.toFixed(0)+'%  '+fmt(sent/sec)+'  '+CNT,'#9ad');"
        "  }"
        // 同一批上传共享的序号与总数，由 onchange 在每次开始时赋值
        "  var CNT=(idx+1)+'/'+all;"
        "  xhr.open('POST','/upload?name='+encodeURIComponent(file.name||'image.jpg'));"
        "  xhr.upload.onprogress=function(e){" 
        "    sent=e.loaded||0;"
        "    if(e.lengthComputable&&e.total>0)total=e.total;"
        "  };"
        // 定时器只描述进度文本，不参与传输控制；无论 onprogress 触发多少次都能刷屏
        "  timer=setInterval(tick,500);"
        "  function finish(err){"
        "    if(timer){clearInterval(timer);timer=0;}"
        // 服务端已确认入队：此时才把进度定格到 100%，避免传输中提前显示完成
        "    if(!err){"
        "      finished=1;"
        "      shown=100;"
        // 计算平均速度需要发送体长度，这里按与 tick 相同的口径取分母
        "      var len=total>0?total:((file.size||0)+MIME_PAD);"
        "      var sec=((new Date()).getTime()-t0)/1000;"
        "      if(sec<0.1)sec=0.1;"
        "      say('100%  '+fmt(len/sec)+'  '+CNT,'#9ad');"
        "    }"
        "    done(err);"
        "  }"
        "  xhr.onload=function(){"
        "    if(xhr.status<200||xhr.status>=300){" 
        "      finish(xhr.status+' '+xhr.responseText);"
        "      return;"
        "    }"
        "    var seq=xhr.getResponseHeader('X-Image-Seq')||'?';"
        "    if(seq==='?'){"
        "      finish('\xE6\x9C\x8D\xE5\x8A\xA1\xE5\x99\xA8\xE6\x9C\xAA\xE5\x85\xA5\xE9\x98\x9F');"
        "      return;"
        "    }"
        "    finish(null);"
        "  };"
        "  xhr.onerror=function(){finish('\xE7\xBD\x91\xE7\xBB\x9C\xE9\x94\x99\xE8\xAF\xAF');};"
        "  xhr.onabort=function(){finish('\xE5\xB7\xB2\xE5\x8F\x96\xE6\xB6\x88');};"
        "  var fd=new FormData();"
        "  fd.append('file',file,file.name);"
        "  xhr.send(fd);"
        "}"
        "f.onchange=function(){"
        "  var files=Array.prototype.slice.call(f.files||[]);"
        "  if(!files.length)return;"
        "  var i=0,bad=0;"
        // 总数固定为本次选中的文件数，当前序号从 1 开始，拼接成「当前/总数」
        "  all=files.length;"
        "  function next(){"
        "    if(i>=files.length){"
         "      f.value='';"
         "      confirmPending(bad);"
        "      return;"
        "    }"
        // idx 记录当前正在传输的文件下标，send 内部按 idx+1 显示「当前序号」
        "    idx=i;"
        "    var file=files[i++];"
        "    say('0%  0 KB/s  '+(idx+1)+'/'+all,'#9ad');"
         "    send(file,function(err){"
         "      if(err)bad++;"
         "      next();"
         "    });"
         "  }"
         "  next();"
         "};"
        // 只回显整体结果，不再暴露具体文件名
        "function confirmPending(bad){"
        "  fetch('/pending',{cache:'no-store'})"
        "    .then(function(r){return r.json();})"
        "    .then(function(j){"
        "      if(bad>0){"
        "        say('\xE5\xAE\x8C\xE6\x88\x90\xEF\xBC\x8C'+bad+' \xE4\xB8\xAA\xE5\xA4\xB1\xE8\xB4\xA5  '+all+'/'+all,'#e88');"
        "      }"
        "      else{"
        "        say('100%  \xE5\xB7\xB2\xE5\xAE\x8C\xE6\x88\x90  '+all+'/'+all,'#ad9');"
        "      }"
        "    })"
        "    .catch(function(){"
        "      say(bad>0?('\xE5\xAE\x8C\xE6\x88\x90\xEF\xBC\x8C'+bad+' \xE4\xB8\xAA\xE5\xA4\xB1\xE8\xB4\xA5  '+all+'/'+all):('100%  \xE5\xB7\xB2\xE5\xAE\x8C\xE6\x88\x90  '+all+'/'+all),'#ad9');"
        "    });"
        "}"
        "</script></body></html>";

    // 循环发送，避免单次 send 未发完导致大图片传输被截断
    // extraHeaders 为可选的额外响应头（每行需自带 \r\n），必须放在空行之前
    void httpResponse(SOCKET s, int code, const std::string& codeText,
        const std::string& contentType,
        const std::vector<uint8_t>& body,
        const std::string& extraHeaders = std::string())
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << code << " " << codeText << "\r\n"
            << "Content-Type: " << contentType << "\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Cache-Control: no-store\r\n"
            << "Connection: close\r\n";
            // 手机端 fetch 读取自定义头（X-Image-Seq / X-Image-Type）需要显式放行，
            // 同源访问时这几行无副作用，跨域（cloudflared 公网地址）时是必需的。
        oss << "Access-Control-Allow-Origin: *\r\n"
            << "Access-Control-Expose-Headers: X-Image-Seq, X-Image-Type, X-Image-Count\r\n";
        // 额外头部拼在空行之前，否则会被客户端当成响应体内容
        if (!extraHeaders.empty()) oss << extraHeaders;
        oss << "\r\n";
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

    /// <summary>
    /// 把字符串统一转成小写，用于头部名 / 后缀比较。
    /// </summary>
    /// <param name="s">待转换的字符串。</param>
    /// <returns>全部小写后的副本。</returns>
    std::string toLower(const std::string& s)
    {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return out;
    }

    /// <summary>
    /// 去掉字符串两端的空格、制表符与回车换行。
    /// </summary>
    /// <param name="s">待处理的字符串。</param>
    /// <returns>去除首尾空白后的副本；全为空白时返回空串。</returns>
    std::string trim(const std::string& s)
    {
        size_t b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return "";
        size_t e = s.find_last_not_of(" \t\r\n");
        return s.substr(b, e - b + 1);
    }

    /// <summary>
    /// 对查询串做百分号解码，并把 '+' 还原成空格。
    /// </summary>
    /// <param name="value">已按 '=' 切分出的原始参数值。</param>
    /// <returns>解码后的字符串。</returns>
    std::string urlDecode(const std::string& value)
    {
        std::string decoded;
        decoded.reserve(value.size());

        for (size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '%' && i + 2 < value.size() &&
                std::isxdigit(static_cast<unsigned char>(value[i + 1])) &&
                std::isxdigit(static_cast<unsigned char>(value[i + 2])))
            {
                decoded.push_back(static_cast<char>(
                    std::stoi(value.substr(i + 1, 2), nullptr, 16)));
                i += 2;
            }
            else if (value[i] == '+')
            {
                decoded.push_back(' ');
            }
            else
            {
                decoded.push_back(value[i]);
            }
        }
        return decoded;
    }

    /// <summary>
    /// 解码 HTTP 分块传输编码（Transfer-Encoding: chunked）的请求体。
    /// </summary>
    /// <param name="raw">分块编码的原始字节（形如 "3e8\r\n<数据>\r\n0\r\n\r\n"）。</param>
    /// <returns>解码后的纯数据字节；格式非法时返回已成功解出的部分。</returns>
    std::vector<uint8_t> decodeChunked(const std::vector<uint8_t>& raw)
    {
        std::vector<uint8_t> out;

        // 起始处就是 "0\r\n" 说明客户端发的是空块，直接返回空数据
        if (raw.size() < 3 || (raw[0] == '0' && raw[1] == '\r' && raw[2] == '\n'))
            return out;

        out.reserve(raw.size());
        size_t pos = 0;
        while (pos < raw.size())
        {
            // 定位本块长度行的结尾 \r\n
            size_t lineEnd = pos;
            while (lineEnd + 1 < raw.size() &&
                !(raw[lineEnd] == '\r' && raw[lineEnd + 1] == '\n'))
                ++lineEnd;
            if (lineEnd + 1 >= raw.size()) break;

            std::string sizeLine(reinterpret_cast<const char*>(raw.data() + pos), lineEnd - pos);

            // 长度行可能带扩展段 "size;name=value"，只取分号前的十六进制长度
            size_t semi = sizeLine.find(';');
            if (semi != std::string::npos) sizeLine = sizeLine.substr(0, semi);

            sizeLine = trim(sizeLine);
            if (sizeLine.empty()) break;

            char* endPtr = nullptr;
            unsigned long long chunkSize = std::strtoull(sizeLine.c_str(), &endPtr, 16);
            if (endPtr == sizeLine.c_str() || chunkSize == 0) break; // 0 号块 = 数据结束

            size_t dataStart = lineEnd + 2;
            if (dataStart + static_cast<size_t>(chunkSize) > raw.size()) break;

            out.insert(out.end(), raw.begin() + dataStart,
                raw.begin() + dataStart + static_cast<size_t>(chunkSize));

            pos = dataStart + static_cast<size_t>(chunkSize);
            // 跳过块尾的 \r\n
            if (pos + 1 < raw.size() && raw[pos] == '\r' && raw[pos + 1] == '\n') pos += 2;
        }

        return out;
    }

    /// <summary>
    /// 从查询串中取出指定参数的值（做过百分号解码）。
    /// </summary>
    /// <param name="query">URL 中 '?' 之后的原始查询串。</param>
    /// <param name="key">参数名。</param>
    /// <returns>参数值；不存在时返回空串。</returns>
    std::string queryParam(const std::string& query, const std::string& key)
    {
        size_t pos = 0;
        while (pos < query.size())
        {
            size_t amp = query.find('&', pos);
            if (amp == std::string::npos) amp = query.size();

            std::string pair = query.substr(pos, amp - pos);
            size_t eq = pair.find('=');
            if (eq != std::string::npos && pair.compare(0, eq, key) == 0)
                return urlDecode(pair.substr(eq + 1));

            pos = amp + 1;
        }
        return "";
    }

    /// <summary>
    /// 按图片头部魔数判断 MIME 类型，在缺少文件名/扩展名时作为兜底。
    /// </summary>
    /// <param name="d">图片字节。</param>
    /// <returns>MIME 类型；无法识别时返回空串。</returns>
    std::string sniffImageMime(const std::vector<uint8_t>& d)
    {
        if (d.size() >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return "image/jpeg";
        if (d.size() >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') return "image/png";
        if (d.size() >= 3 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F') return "image/gif";
        if (d.size() >= 12 && d[0] == 'R' && d[1] == 'I' && d[2] == 'F' && d[3] == 'F' &&
            d[8] == 'W' && d[9] == 'E' && d[10] == 'B' && d[11] == 'P') return "image/webp";
        if (d.size() >= 2 && d[0] == 'B' && d[1] == 'M') return "image/bmp";
        if (d.size() >= 8 && d[4] == 'f' && d[5] == 't' && d[6] == 'y' && d[7] == 'p')
            return "image/heic";
        return "";
    }

    /// <summary>
    /// 按文件名后缀推断图片 MIME 类型。
    /// </summary>
    /// <param name="name">上传的文件名。</param>
    /// <returns>MIME 类型；无法识别时返回空串。</returns>
    std::string mimeFromName(const std::string& name)
    {
        std::string lower = toLower(name);

        auto endsWith = [&lower](const char* ext)
        {
            size_t n = std::strlen(ext);
            return lower.size() >= n && lower.compare(lower.size() - n, n, ext) == 0;
        };

        if (endsWith(".jpg") || endsWith(".jpeg")) return "image/jpeg";
        if (endsWith(".png"))  return "image/png";
        if (endsWith(".gif"))  return "image/gif";
        if (endsWith(".webp")) return "image/webp";
        if (endsWith(".bmp"))  return "image/bmp";
        if (endsWith(".heic") || endsWith(".heif")) return "image/heic";
        return "";
    }

    /// <summary>
    /// 判断字节流看起来是否是一张图片（用于过滤空体 / 误上传的非图片内容）。
    /// </summary>
    /// <param name="d">待检查的字节。</param>
    /// <returns>识别为已知图片格式返回 true。</returns>
    bool looksLikeImage(const std::vector<uint8_t>& d)
    {
        return !sniffImageMime(d).empty();
    }

    /// <summary>
    /// 把文本渲染成二维码纹理。
    /// </summary>
    /// <param name="textUtf8">要编码的文本（UTF-8）。</param>
    /// <param name="scale">每个二维码模块的像素边长，小于 1 时按 1 处理。</param>
    /// <param name="border">四周空白模块数，负数按 0 处理。</param>
    /// <returns>二维码纹理；文本为空或编码失败时返回空纹理。</returns>
    sf::Texture renderQR(const std::string& textUtf8, int scale, int border)
    {
        if (textUtf8.empty()) return {};

        // 参数兜底：0 或负值会生成 0 像素图，sf::Texture 会直接创建失败
        if (scale < 1) scale = 1;
        if (border < 0) border = 0;

        const qrcodegen::QrCode code =
            qrcodegen::QrCode::encodeText(textUtf8.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);

        const int modules = code.getSize();
        const int dim = modules + border * 2;
        const int px = dim * scale;

        sf::Image img({ static_cast<unsigned>(px), static_cast<unsigned>(px) }, sf::Color::White);

        for (int my = 0; my < modules; ++my)
        {
            for (int mx = 0; mx < modules; ++mx)
            {
                if (!code.getModule(mx, my)) continue;

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
} // namespace

// ================= HTML 文件读取 =================

std::string ShareImage::loadHTML() const
{
    std::wstring tryPaths[] = {
        htmlPath_,
        L"Net/ShareImage.html",
        L"./Net/ShareImage.html",
        L"../Net/ShareImage.html",
    };

    FILE* f = nullptr;
    std::wstring foundPath;

    for (const auto& p : tryPaths)
    {
        if (p.empty()) continue;
        f = _wfopen(p.c_str(), L"rb");
        if (f) { foundPath = p; break; }
    }

    // 相对路径可能基于当前工作目录，再尝试一次以可执行文件目录为基准的路径
    if (!f)
    {
        wchar_t exePath[MAX_PATH] = { 0 };
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0)
        {
            std::wstring dir(exePath);
            size_t slash = dir.find_last_of(L"\\/");
            if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);
            std::wstring full = dir + L"Net/ShareImage.html";
            f = _wfopen(full.c_str(), L"rb");
            if (f) foundPath = full;
        }
    }

    // 磁盘上没有自定义页面时退回内置最小页面，保证功能可用
    if (!f)
    {
        std::cout << "[ShareImage] Net/ShareImage.html not found, use built-in page" << std::endl;
        return std::string(kDefaultHTML);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0)
    {
        fclose(f);
        return std::string(kDefaultHTML);
    }

    std::string content(static_cast<size_t>(size), '\0');
    fread(&content[0], 1, static_cast<size_t>(size), f);
    fclose(f);

    std::cout << "[ShareImage] Loaded HTML: "
        << xs::XString::Convert::wstring_to_utf8(foundPath)
        << " (" << size << " bytes)" << std::endl;

    return content;
}

const std::string& ShareImage::getHTML()
{
    std::lock_guard<std::mutex> lock(htmlMutex_);
    if (!htmlLoaded_)
    {
        cachedHTML_ = loadHTML();
        htmlLoaded_ = true;
    }
    return cachedHTML_;
}

void ShareImage::reloadHTML()
{
    std::lock_guard<std::mutex> lock(htmlMutex_);
    cachedHTML_ = loadHTML();
    htmlLoaded_ = true;
    std::cout << "[ShareImage] HTML reloaded" << std::endl;
}

// ================= 构造 / 析构 =================

void ShareImage::Init(int port)
{
    port_ = port;

#ifdef _WIN32
    WSADATA wsa;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (result != 0)
    {
        std::cerr << "[ShareImage] WSAStartup failed, err=" << result << std::endl;
        return;
    }
    std::cout << "[ShareImage] WSA initialized, port=" << port_ << std::endl;
#endif
}

ShareImage::~ShareImage()
{
    stop();          // 统一在 stop() 中关闭服务器并清空队列
#ifdef _WIN32
    WSACleanup();
#endif
}

// ================= 生命周期 =================

bool ShareImage::start()
{
    return startServer();
}

void ShareImage::stop()
{
    // 先停隧道再停 HTTP 服务：避免隧道仍在转发时本地套接字已被关闭
    stopTunnel();

    if (!running_) return;

    running_ = false;
    stopRequested_ = true;

    // 关闭监听套接字让阻塞在 accept 的线程退出
    if (serverSocket_ != INVALID_SOCKET)
    {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }

    if (serverThread_.joinable())
        serverThread_.join();

    // 停止服务时【不清空】上传队列。
    // 旧实现在此处 swap 掉 uploadQueue_，而 PC 端的取图流程正是
    // 「先 stop() 再 PollShareImage()」，结果手机明明上传成功、
    // 队列却在取之前被清空，表现为「手机显示成功但电脑端没有图片」。
    // 队列的释放交给下一次 start()（见 startServer 中的清理）或对象析构。

    std::cout << "[ShareImage] stopped" << std::endl;
}

bool ShareImage::startServer()
{
    if (running_) stop();

    buildURL();

#ifdef _WIN32
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == INVALID_SOCKET)
    {
        std::cerr << "[ShareImage] socket() failed: " << WSAGetLastError() << std::endl;
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
        std::cerr << "[ShareImage] bind() failed on port " << port_
            << " error=" << WSAGetLastError() << std::endl;
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }

    if (listen(serverSocket_, 8) == SOCKET_ERROR)
    {
        std::cerr << "[ShareImage] listen() failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }

    std::cout << "[ShareImage] listening on port " << port_ << std::endl;
#else
    std::cerr << "[ShareImage] not supported on this platform" << std::endl;
    return false;
#endif

    stopRequested_ = false;
    exitRequested_ = false;
    running_ = true;
    serverThread_ = std::thread([this]() { runServer(); });
    return true;
}

void ShareImage::runServer()
{
    std::cout << "[ShareImage] server thread started" << std::endl;

    while (running_)
    {
        sockaddr_in client{};
        int len = sizeof(client);
        SOCKET cs = accept(serverSocket_, (sockaddr*)&client, &len);

        if (cs == INVALID_SOCKET)
        {
            if (!running_) break;

            // accept 失败通常是套接字已被关闭，短暂休眠避免空转占满 CPU
            SLEEP_MS(10);
            continue;
        }

        // 每个连接一个线程：上传大图耗时可能较长，不能阻塞其它请求
        std::thread([this, cs]() { handleClient(cs); }).detach();
    }

    std::cout << "[ShareImage] server stopped" << std::endl;
}

// ================= HTTP 请求分发 =================

void ShareImage::handleClient(SOCKET s)
{
    // 设置收发超时：手机端建连后中途断开（切后台 / 锁屏）时 recv 不再永久阻塞，
    // 否则每个断开的客户端都卡一个永久阻塞的分离线程，句柄与内存持续泄漏。
#ifdef _WIN32
    DWORD recvTimeout = 15000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&recvTimeout), sizeof(recvTimeout));

    // 发送超时同样必要：手机端提前关闭连接时 send 否则会长时间阻塞
    DWORD sendTimeout = 15000;
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO,
        reinterpret_cast<const char*>(&sendTimeout), sizeof(sendTimeout));

    // 关闭 Nagle：上传请求多为「头部 + 小块 body」，延迟合并反而增加等待
    int nodelay = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
        reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
#endif

    auto raw = readHttpRequest(s);
    if (raw.empty()) { closesocket(s); return; }

    // 提取请求行（第一行 \r\n 之前的内容）以拿到 method 与 path
    std::string reqLine;
    size_t eol = raw.size();
    for (size_t i = 0; i + 1 < raw.size(); ++i)
    {
        if (raw[i] == '\r' && raw[i + 1] == '\n') { eol = i; break; }
    }
    reqLine = std::string((const char*)raw.data(), eol);

    std::string method, target;
    {
        std::istringstream iss(reqLine);
        iss >> method >> target;
    }

    // 请求行里的 target 形如 "/upload?name=a.jpg"，必须拆出 path 与查询串。
    // 旧代码把整个 target 当成 path 比较，导致浏览器带任何查询串时所有路由都落到 404。
    std::string path = target;
    std::string query;
    size_t qpos = target.find('?');
    if (qpos != std::string::npos)
    {
        path = target.substr(0, qpos);
        query = target.substr(qpos + 1);
    }

    const std::string clenStr = parseHeader(raw, "Content-Length");
    const size_t clen = clenStr.empty() ? 0 : (size_t)std::atoll(clenStr.c_str());
    const std::string ctype = parseHeader(raw, "Content-Type");
    const std::string transferEncoding = toLower(parseHeader(raw, "Transfer-Encoding"));
    const bool chunked = transferEncoding.find("chunked") != std::string::npos;

    std::vector<uint8_t> body = parseBody(raw);

    // 只有 POST / PUT / PATCH 才有请求体。GET 若也进入补齐循环，
    // 会白等一次 15 秒 recv 超时，页面首屏肉眼可见地卡顿。
    const bool hasBody = (method == "POST" || method == "PUT" || method == "PATCH");

    if (hasBody && !chunked)
    {
        // 首次 recv 只拿到部分 body，继续补齐到 Content-Length 指定长度。
        // 条件必须固定写成 body.size() < clen：旧写法靠 recv 返回 0 才退出，
        // 最终把所有 recv 都撞到超时，上传耗时被拖成十几个 15 秒。
        while (body.size() < clen && running_)
        {
            uint8_t tmp[16384];
            int n = recv(s, (char*)tmp, sizeof(tmp), 0);
            if (n <= 0) break;
            body.insert(body.end(), tmp, tmp + n);
        }

        // 客户端可能多发了尾部字节（极少见），按 Content-Length 截断保证图片字节干净
        if (clen > 0 && body.size() > clen) body.resize(clen);
    }
    else if (hasBody && chunked)
    {
        // chunked 没有 Content-Length，必须读到「0 长度块」才算结束。
        // 若不解码，body 里是 "长度\r\n数据\r\n" 的原始块，PC 端解码图片必然失败，
        // 表现正好就是「手机显示上传完成但电脑端没有」。
        auto hasFinalChunk = [](const std::vector<uint8_t>& d)
        {
            if (d.size() >= 3 && d[0] == '0' && d[1] == '\r' && d[2] == '\n')
                return true;
            const std::string tail((const char*)d.data(), d.size());
            return tail.find("\r\n0\r\n") != std::string::npos;
        };

        while (running_ && !hasFinalChunk(body))
        {
            uint8_t tmp[16384];
            int n = recv(s, (char*)tmp, sizeof(tmp), 0);
            if (n <= 0) break;
            body.insert(body.end(), tmp, tmp + n);
        }

        body = decodeChunked(body);
    }
    else if (!hasBody)
    {
        // GET / HEAD 等无体请求：忽略头部之后可能残留的空内容
        body.clear();
    }

    std::cout << "[ShareImage] " << method << " " << path
        << " body=" << body.size() << std::endl;

    if (path == "/" || path == "/index.html")
        handlePage(s);
    else if (path == "/upload")
        handleUpload(s, body, ctype, queryParam(query, "name"));
    else if (path == "/status")
        handleStatus(s);
    else if (path == "/pending")
        handlePending(s);
    else if (path == "/stop")
        handleStop(s);
    else if (path == "/exit")
        handleExit(s);
    else if (path == "/admin/reload")
    {
        reloadHTML();
        httpResponseText(s, "reloaded");
    }
    else if (path == "/favicon.ico")
        httpResponse(s, 404, "Not Found", "text/plain", {});
    else
        // 404 响应体必须是字符串字面量的字节，不能写成 { char, 1 }，否则内容错误
        httpResponse(s, 404, "Not Found", "text/plain",
            std::vector<uint8_t>{ '4', '0', '4' });

    closesocket(s);
}

void ShareImage::handlePage(SOCKET s)
{
    const std::string& html = getHTML();
    httpResponse(s, 200, "OK", "text/html; charset=utf-8",
        std::vector<uint8_t>(html.begin(), html.end()));
}

void ShareImage::handleUpload(SOCKET s, const std::vector<uint8_t>& body,
    const std::string& contentType,
    const std::string& fileName)
{
    if (body.empty())
    {
        std::cerr << "[ShareImage] upload rejected: empty body" << std::endl;
        httpResponse(s, 400, "Bad Request", "text/plain; charset=utf-8",
            std::vector<uint8_t>{ 'e', 'm', 'p', 't', 'y' });
        return;
    }

    SharedImage img;
    std::string name = fileName;       // 上传原始文件名，用于推断格式
    std::string partType;              // multipart 中声明的 MIME

    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        const std::string bound = "boundary=";
        std::string boundary;
        auto pos = contentType.find(bound);
        if (pos != std::string::npos)
        {
            boundary = contentType.substr(pos + bound.size());
            boundary.erase(std::remove(boundary.begin(), boundary.end(), '"'), boundary.end());

            // boundary 后面可能紧跟 ';' 分隔的其他参数（charset 等），必须截断，
            // 否则拼接出的分隔符 "--boundary;charset=..." 永远匹配不到真实分隔符。
            size_t semi = boundary.find(';');
            if (semi != std::string::npos) boundary = boundary.substr(0, semi);

            boundary = trim(boundary);
        }

        if (boundary.empty())
            std::cerr << "[ShareImage] multipart without boundary, fallback to raw body" << std::endl;
        else
            img = parseMultipart(body, boundary);

        // multipart 解析失败时退回「整段 body 当图片」，避免明明收到图却因边界识别问题被丢弃
        if (!img.valid && looksLikeImage(body))
        {
            img.data = body;
            img.valid = true;
        }
    }
    else
    {
        // 裸 body 上传（旧页面 / 第三方客户端）：整段字节就是图片
        img.data = body;
        img.valid = true;
    }

    // 额外记录内容类型，供 PC 端解码时参考：优先客户端声明的 image/*，
    // 其次按文件名后缀推断，最后按字节魔数嗅探（应对 name/type 都缺失的请求）。
    if (contentType.compare(0, 6, "image/") == 0)
        partType = trim(contentType);
    if (partType.empty()) partType = mimeFromName(name);
    if (partType.empty() && img.valid) partType = sniffImageMime(img.data);

    // 完全空数据视为无效（例如请求体只有分隔符），不入队以免污染队列
    if (img.valid && img.data.empty()) img.valid = false;

    // 序号在成员函数中统一分配，避免 static 解析函数触碰非静态成员
    if (img.valid) img.seq = ++seqGen_;

    if (img.valid)
    {
        img.name = name;
        img.mimeType = partType;

        size_t pending = 0;
        {
            std::lock_guard<std::mutex> lk(uploadMutex_);
            uploadQueue_.push(img);
            pending = uploadQueue_.size();
        }

        // 累计计数独立于队列：队列被 PC 端取空后该值仍然保留，
        // 供 getReceivedImageCount() 给界面显示「本次共收到多少张」。
        ++receivedCount_;

        // 入队日志刻意打印 seq / 队列长度 / 文件名 / 类型，
        // 手机端显示成功后，可在 PC 控制台直接确认「图确实进了队列」。
        std::cout << "[ShareImage] image queued (seq=" << img.seq
            << ", pending=" << pending
            << ", bytes=" << img.data.size()
            << ", received=" << receivedCount_.load()
            << ", name=" << (img.name.empty() ? "(none)" : img.name)
            << ", type=" << (img.mimeType.empty() ? "(unknown)" : img.mimeType)
            << ")" << std::endl;
    }
    else
    {
        std::cerr << "[ShareImage] invalid image body ignored (bytes="
            << body.size() << ")" << std::endl;
    }

    // 回给网页端时带上 X-Image-Seq / X-Image-Type / X-Image-Count，
    // 便于页面显示进度与排查；X-Image-Count 为本次运行累计接收张数。
    std::ostringstream extra;
    if (img.valid)
    {
        extra << "X-Image-Seq: " << img.seq << "\r\n";
        extra << "X-Image-Count: " << receivedCount_.load() << "\r\n";
        if (!img.mimeType.empty()) extra << "X-Image-Type: " << img.mimeType << "\r\n";
    }

    const std::string text = img.valid ? "uploaded" : "invalid";
    httpResponse(s, img.valid ? 200 : 400, img.valid ? "OK" : "Bad Request",
        "text/plain; charset=utf-8",
        std::vector<uint8_t>(text.begin(), text.end()),
        extra.str());
}

void ShareImage::handleStatus(SOCKET s)
{
    // 只统计队列长度，直接在锁外拼装字符串，避免网络异常时长时间持锁
    size_t pending = pendingCount();
    httpResponseText(s, "running=" + std::string(running_ ? "1" : "0")
        + " pending=" + std::to_string(pending));
}

void ShareImage::handlePending(SOCKET s)
{
    // 不消费队列，只做快照，方便手机端确认「服务器真的收到了图」
    std::vector<std::pair<uint32_t, size_t>> items;
    {
        std::lock_guard<std::mutex> lk(uploadMutex_);
        std::queue<SharedImage> copy = uploadQueue_;
        while (!copy.empty())
        {
            items.emplace_back(copy.front().seq, copy.front().data.size());
            copy.pop();
        }
    }

    std::ostringstream oss;
    oss << "{\"pending\":" << items.size() << ",\"items\":[";
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i) oss << ',';
        oss << "{\"seq\":" << items[i].first << ",\"bytes\":" << items[i].second << "}";
    }
    oss << "]}";

    const std::string json = oss.str();
    httpResponse(s, 200, "OK", "application/json; charset=utf-8",
        std::vector<uint8_t>(json.begin(), json.end()));
}

void ShareImage::handleStop(SOCKET s)
{
    stopRequested_ = true;
    httpResponseText(s, "stopped");
}

void ShareImage::handleExit(SOCKET s)
{
    exitRequested_ = true;
    stopRequested_ = true;
    httpResponseText(s, "bye");
}

// ================= 请求解析 =================

std::vector<uint8_t> ShareImage::readHttpRequest(SOCKET s)
{
    std::vector<uint8_t> raw;
    char buf[8192];

    // 先读到头部结束标记 \r\n\r\n，body 可能随头部一起到达
    while (true)
    {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) return raw;

        raw.insert(raw.end(), buf, buf + n);

        if (raw.size() >= 4)
        {
            // 只回扫本次新到达数据前 3 个字节，避免分隔符跨 recv 边界时漏检；
            // 起始下标下限取 0，防止 raw.size() < n 时无符号下溢成超大值导致越界/漏检
            const size_t start = raw.size() > static_cast<size_t>(n) + 3
                ? raw.size() - static_cast<size_t>(n) - 3
                : 0;
            for (size_t i = start; i + 3 < raw.size(); ++i)
            {
                if (raw[i] == '\r' && raw[i + 1] == '\n' &&
                    raw[i + 2] == '\r' && raw[i + 3] == '\n')
                    return raw;
            }
        }

        // 头部异常大，防止无限累积
        if (raw.size() > 1024 * 1024) return raw;
    }
}

std::string ShareImage::parseHeader(const std::vector<uint8_t>& raw, const std::string& key)
{
    std::string text((const char*)raw.data(), raw.size());

    size_t headerEnd = text.find("\r\n\r\n");
    if (headerEnd == std::string::npos) headerEnd = text.size();

    std::string lowerKey = key;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });

    size_t pos = 0;
    while (pos < headerEnd)
    {
        size_t lineEnd = text.find("\r\n", pos);
        if (lineEnd == std::string::npos || lineEnd > headerEnd) lineEnd = headerEnd;

        std::string line = text.substr(pos, lineEnd - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            std::string name = line.substr(0, colon);
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });

            if (name == lowerKey)
            {
                std::string value = line.substr(colon + 1);
                size_t b = value.find_first_not_of(" \t");
                size_t e = value.find_last_not_of(" \t\r");
                if (b == std::string::npos) return "";
                if (e == std::string::npos || e < b) e = value.size() - 1;
                return value.substr(b, e - b + 1);
            }
        }

        pos = lineEnd + 2;
    }

    return "";
}

std::vector<uint8_t> ShareImage::parseBody(const std::vector<uint8_t>& raw)
{
    // 找到头部结束位置，其后全部视为 body
    for (size_t i = 0; i + 3 < raw.size(); ++i)
    {
        if (raw[i] == '\r' && raw[i + 1] == '\n' &&
            raw[i + 2] == '\r' && raw[i + 3] == '\n')
            return std::vector<uint8_t>(raw.begin() + i + 4, raw.end());
    }
    return {};
}

SharedImage ShareImage::parseMultipart(const std::vector<uint8_t>& body,
    const std::string& boundary)
{
    SharedImage img;
    if (boundary.empty()) return img;

    std::string data((const char*)body.data(), body.size());
    std::string delim = "--" + boundary;

    size_t pos = data.find(delim);
    if (pos == std::string::npos) return img;

    // 逐个 part 扫描，取第一个带文件内容（存在 filename=）的部分
    while (true)
    {
        size_t partStart = data.find("\r\n\r\n", pos);
        if (partStart == std::string::npos) break;

        size_t contentStart = partStart + 4;
        size_t nextDelim = data.find(delim, contentStart);
        if (nextDelim == std::string::npos) break;

        std::string headers = data.substr(pos, partStart - pos);
        bool isFile = headers.find("filename=") != std::string::npos;

        if (isFile)
        {
            // part 内容末尾的 \r\n 属于分隔符的一部分，需去掉
            size_t contentEnd = nextDelim;
            if (contentEnd >= 2 && data[contentEnd - 2] == '\r' && data[contentEnd - 1] == '\n')
                contentEnd -= 2;

            if (contentEnd > contentStart)
            {
                img.data.assign(body.begin() + contentStart, body.begin() + contentEnd);
                // 本函数为 static，不得访问非静态成员 seqGen_，序号统一由调用方 handleUpload 分配
                img.valid = true;
                return img;
            }
        }

        pos = nextDelim;
    }

    return img;
}

// ================= 地址 / 二维码 =================

std::string ShareImage::getLocalIP() const
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

    char ip[INET_ADDRSTRLEN] = { 0 };
    // inet_ntop 失败时 ip 保持全 0，直接返回会拼出非法地址，故回退到回环地址
    if (inet_ntop(AF_INET, &localAddr.sin_addr, ip, sizeof(ip)) == nullptr)
    {
        closesocket(s);
        return "127.0.0.1";
    }
    closesocket(s);
    return std::string(ip);
}

void ShareImage::buildURL()
{
    std::string ip = getLocalIP();
    std::lock_guard<std::mutex> lk(urlMutex_);
    url_ = L"http://" + xs::XString::Convert::utf8_to_wstring(ip)
        + L":" + std::to_wstring(port_) + L"/";
}

std::wstring ShareImage::getURL() const
{
    std::lock_guard<std::mutex> lk(urlMutex_);
    return url_;
}

// ================= 公网穿透（cloudflared） =================

bool ShareImage::startTunnel()
{
    if (tunnelRunning_) return true;

    // 未启动 HTTP 服务时隧道转发不到任何内容，先启动本地服务
    if (!running_ && !start()) return false;

    // 与 ShareFile 保持一致：cloudflared.exe 放在可执行文件旁的 Net 目录
    wchar_t exeDir[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
    std::wstring exePath(exeDir);
    size_t lastSlash = exePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) exePath = exePath.substr(0, lastSlash);
    std::wstring cfPath = exePath + L"\\Net\\cloudflared.exe";

    if (_waccess(cfPath.c_str(), 0) != 0)
    {
        std::cerr << "[ShareImage] cloudflared.exe not found at: "
            << xs::XString::Convert::wstring_to_utf8(cfPath) << std::endl;
        return false;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&tunnelStdoutRead_, &tunnelStdoutWrite_, &sa, 0))
    {
        std::cerr << "[ShareImage] CreatePipe failed" << std::endl;
        return false;
    }

    // 读端不能被 cloudflared 进程继承，否则关闭写端后仍读不到 EOF
    SetHandleInformation(tunnelStdoutRead_, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.hStdOutput = tunnelStdoutWrite_;
    si.hStdError = tunnelStdoutWrite_;
    si.dwFlags |= STARTF_USESTDHANDLES;

    std::wstring cmdline = L"\"" + cfPath + L"\" tunnel --url http://localhost:"
        + std::to_wstring(port_);

    ZeroMemory(&tunnelProc_, sizeof(tunnelProc_));
    if (!CreateProcessW(cfPath.c_str(), &cmdline[0], nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &tunnelProc_))
    {
        std::cerr << "[ShareImage] CreateProcess failed: " << GetLastError() << std::endl;
        CloseHandle(tunnelStdoutRead_);  tunnelStdoutRead_ = nullptr;
        CloseHandle(tunnelStdoutWrite_); tunnelStdoutWrite_ = nullptr;
        return false;
    }

    std::cout << "[ShareImage] cloudflared started (PID="
        << tunnelProc_.dwProcessId << ")" << std::endl;

    {
        std::lock_guard<std::mutex> lk(publicUrlMutex_);
        publicURL_.clear();
    }

    // 等待 cloudflared 打印公网地址，最多约 15 秒（30 次 × 500ms）
    for (int i = 0; i < 30; ++i)
    {
        Sleep(500);

        std::string output = readTunnelOutput();
        if (!output.empty())
        {
            // 只认 trycloudflare.com 域名，避免把日志里其他 https 链接当成隧道地址
            size_t pos = output.find("https://");
            while (pos != std::string::npos)
            {
                size_t end = output.find_first_of("\r\n \t\"'", pos);
                if (end == std::string::npos) end = output.size();

                std::string candidate = output.substr(pos, end - pos);
                if (candidate.find("trycloudflare.com") != std::string::npos)
                {
                    // 去掉末尾斜杠后统一补一个根路径，服务器会忽略 path
                    if (!candidate.empty() && candidate.back() == '/')
                        candidate.pop_back();

                    {
                        std::lock_guard<std::mutex> lk(publicUrlMutex_);
                        publicURL_ = xs::XString::Convert::utf8_to_wstring(candidate) + L"/";
                    }
                    std::wcout << L"[ShareImage] Public URL: "
                        << xs::XString::Convert::utf8_to_wstring(candidate)
                        << L"/" << std::endl;
                    break;
                }

                pos = output.find("https://", end);
            }

            if (isTunnelRunning()) break;
        }

        DWORD exitCode = 0;
        if (GetExitCodeProcess(tunnelProc_.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
        {
            std::cerr << "[ShareImage] cloudflared exited early, code=" << exitCode << std::endl;
            break;
        }
    }

    if (getPublicURL().empty())
    {
        std::cerr << "[ShareImage] Failed to get tunnel URL (timeout)" << std::endl;
        stopTunnel();
        return false;
    }

    tunnelRunning_ = true;
    return true;
}

std::string ShareImage::readTunnelOutput()
{
    std::string result;
    if (!tunnelStdoutRead_) return result;

    char buf[1024];
    DWORD available = 0;
    PeekNamedPipe(tunnelStdoutRead_, nullptr, 0, nullptr, &available, nullptr);

    // 每次最多读 8KB，避免 cloudflared 输出过快时长时间阻塞在这里
    while (available > 0 && result.size() < 8192)
    {
        DWORD bytesRead = 0;
        DWORD toRead = (available < sizeof(buf) - 1) ? available : (DWORD)sizeof(buf) - 1;
        if (!ReadFile(tunnelStdoutRead_, buf, toRead, &bytesRead, nullptr) || bytesRead == 0)
            break;

        // bytesRead 最大等于 sizeof(buf)-1，末尾仍预留 1 字节写 '\0'，不会越界
        buf[bytesRead] = '\0';
        result += buf;
        PeekNamedPipe(tunnelStdoutRead_, nullptr, 0, nullptr, &available, nullptr);
    }

    if (!result.empty())
        std::cout << "[cf-image] " << result << std::endl;

    return result;
}

void ShareImage::stopTunnel()
{
    if (!tunnelRunning_ && !tunnelProc_.hProcess)
    {
        // 仍可能残留管道句柄（启动失败路径），一并清理
        if (tunnelStdoutRead_) { CloseHandle(tunnelStdoutRead_); tunnelStdoutRead_ = nullptr; }
        if (tunnelStdoutWrite_) { CloseHandle(tunnelStdoutWrite_); tunnelStdoutWrite_ = nullptr; }
        return;
    }

    tunnelRunning_ = false;
    {
        std::lock_guard<std::mutex> lk(publicUrlMutex_);
        publicURL_.clear();
    }

    if (tunnelProc_.hProcess)
    {
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, tunnelProc_.dwProcessId);
        Sleep(1000);

        DWORD exitCode = 0;
        if (GetExitCodeProcess(tunnelProc_.hProcess, &exitCode) && exitCode == STILL_ACTIVE)
            TerminateProcess(tunnelProc_.hProcess, 1);

        CloseHandle(tunnelProc_.hProcess);
        CloseHandle(tunnelProc_.hThread);
        ZeroMemory(&tunnelProc_, sizeof(tunnelProc_));
    }

    if (tunnelStdoutRead_) { CloseHandle(tunnelStdoutRead_); tunnelStdoutRead_ = nullptr; }
    if (tunnelStdoutWrite_) { CloseHandle(tunnelStdoutWrite_); tunnelStdoutWrite_ = nullptr; }

    std::cout << "[ShareImage] Tunnel stopped" << std::endl;
}

std::wstring ShareImage::getPublicURL() const
{
    std::lock_guard<std::mutex> lk(publicUrlMutex_);
    return publicURL_;
}

sf::Texture ShareImage::getPublicQRTexture(int scale, int border) const
{
    std::wstring url = getPublicURL();
    if (url.empty()) return {};

    // 与局域网二维码共用同一套渲染实现，避免两份绘制代码各自漂移
    return renderQR(xs::XString::Convert::wstring_to_utf8(url), scale, border);
}

sf::Texture ShareImage::getQRTexture(int scale, int border) const
{
    std::wstring url;
    {
        std::lock_guard<std::mutex> lk(urlMutex_);
        url = url_;
    }
    if (url.empty()) return {};

    return renderQR(xs::XString::Convert::wstring_to_utf8(url), scale, border);
}

// ================= PC 端取图 =================

bool ShareImage::hasUploadedImage() const
{
    std::lock_guard<std::mutex> lk(uploadMutex_);
    return !uploadQueue_.empty();
}

SharedImage ShareImage::popUploadedImage()
{
    std::lock_guard<std::mutex> lk(uploadMutex_);
    if (uploadQueue_.empty()) return SharedImage();

    SharedImage img = uploadQueue_.front();
    uploadQueue_.pop();
    return img;
}

size_t ShareImage::pendingCount() const
{
    std::lock_guard<std::mutex> lk(uploadMutex_);
    return uploadQueue_.size();
}

std::vector<SharedImage> ShareImage::popAllUploadedImages()
{
    std::vector<SharedImage> out;

    // 整体换出队列再遍历，避免在大图拷贝期间长时间持有锁
    std::queue<SharedImage> local;
    {
        std::lock_guard<std::mutex> lk(uploadMutex_);
        std::swap(local, uploadQueue_);
    }

    out.reserve(local.size());
    while (!local.empty())
    {
        out.push_back(std::move(local.front()));
        local.pop();
    }
    return out;
}

int ShareImage::getReceivedImageCount() const
{
    // 计数是原子变量，无需加锁；它只增不减，与队列当前长度相互独立，
    // 这样即使 PC 端已把队列取空，界面仍能显示本次共收到了多少张。
    return receivedCount_.load();
}

void ShareImage::resetReceivedImageCount()
{
    // 新一轮上传会话开始时清零，避免上一轮的累计值混入
    receivedCount_ = 0;
}

void ShareImage::requestStopStream()
{
    stopRequested_ = true;
}

void ShareImage::requestExitCamera()
{
    exitRequested_ = true;
    stopRequested_ = true;
}

int ShareImage::findAvailablePort(int preferred)
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
