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

// TCP_NODELAY 在部分 SDK 头文件里缺失，这里做一次兜底定义，
// 避免因一个可选优化导致整个文件无法编译（Windows 上该常量由 <winsock2.h> 提供）。
#ifndef TCP_NODELAY
#  define TCP_NODELAY 0x0001
#endif

#ifdef _WIN32
#  define SLEEP_MS(ms) Sleep(ms)
#else
#  include <chrono>
#  include <thread>
#  define SLEEP_MS(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#endif

namespace AA
{
    sf::Texture UpscaleAA(const sf::Texture& src, float scale, float edge, float inner);
    bool        isAvailable();
    void        UpscaleAAInPlace(sf::Texture& t, float scale, float edge, float inner);
}

// ----------------------------------------------------------------
// 传输压缩 / 接收放大 相关常量
// ----------------------------------------------------------------
namespace
{
    /// <summary>
    /// 上传页压缩后允许的最大边长（长边）。1920 是 1920x1080 的长边，
    /// 竖屏照片按同一边长等比缩放，像素总量与 1920x1080 同量级。
    /// </summary>
    constexpr int kMaxUploadSide = 1920;

    /// <summary>
    /// 网页端 JPEG 压缩质量（0.82 对板书文字仍清晰，体积约为原图的 1/5）。
    /// </summary>
    constexpr double kUploadQuality = 0.82;

    /// <summary>
    /// 接收端抗锯齿放大的倍率。压缩到 1920 长边后仅用于屏幕显示，
    /// 放大 2 倍再配合 AA 柔化边缘，观感显著优于小图直接拉伸。
    /// </summary>
    constexpr float kReceiveUpscale = 2.0f;

    /// <summary>
    /// 接收端放大后的最大边长。1920 长边再放大 2 倍会得到约 3840 长边的纹理
    /// （约 33MB/张），多张排队时显存与内存压力过大；这里限制在 2560，
    /// 既保证屏幕观感，又把单张纹理控制在约 15MB 以内。
    /// </summary>
    constexpr int kMaxReceivedSide = 2560;

    /// <summary>
    /// 判断给定 MIME 是否属于「已做过有损压缩」的格式。
    /// </summary>
    /// <param name="mime">图片 MIME 类型。</param>
    /// <returns>JPEG / WebP / HEIC 返回 true。</returns>
    inline bool isLossyMime(const std::string& mime)
    {
        return mime == "image/jpeg" || mime == "image/webp" || mime == "image/heic";
    }

    /// <summary>
    /// 单次 recv 的最大缓冲长度。256KB 可在一个 TCP 窗口内一次收下更多已到达数据，
    /// 2MB 级图片的 recv 次数从 32 次降到 8 次，是接收侧最直接的耗时优化。
    /// </summary>
    constexpr int kRecvChunk = 256 * 1024;

    /// <summary>
    /// 接收侧线程局部缓冲的容量（字节）。
    /// 从 64KB 提到 256KB 后，单次 recv 能一次吞下更多已在 TCP 接收队列里的数据：
    /// 2MB 图片的 recv 次数从 ~32 次降到 ~8 次，每次系统调用与边界检查都被均摊掉，
    /// 是「减少等待处理照片时间」在接收环节最直接的收益。
    /// </summary>
    constexpr size_t kRecvBufferSize = 256 * 1024;

    /// <summary>
    /// 客户端连接槽上限。手机端一次最多并发 10 路，取 16 留出浏览器预连接余量，
    /// 既不会把并发压到 1（串行化上传），也不会无限创建分离线程。
    /// </summary>
    constexpr int kMaxClientSlots = 16;

    /// <summary>
    /// 判断给定字节序列是否以 <paramref name="needle"/> 开头。
    /// </summary>
    /// <param name="data">被检查的数据起始指针。</param>
    /// <param name="size">数据可用长度。</param>
    /// <param name="needle">期望的前缀。</param>
    /// <returns>前缀完全匹配时返回 true。</returns>
    inline bool startsWith(const uint8_t* data, size_t size, const char* needle)
    {
        const size_t n = std::strlen(needle);
        return size >= n && std::memcmp(data, needle, n) == 0;
    }
}

// ----------------------------------------------------------------
// 顶点着色器：属性与矩阵全部由 SFML 注入
// ----------------------------------------------------------------
static const char* kVert = R"GLSL(
    in vec2 sfml_Vertex;
    in vec2 sfml_TexCoord;

    out vec2 vUV;

    uniform mat4 sfml_ProjectionMatrix;
    uniform mat4 sfml_TransformMatrix;

    void main()
    {
        vUV = sfml_TexCoord;
        gl_Position = sfml_ProjectionMatrix * sfml_TransformMatrix * vec4(sfml_Vertex, 0.0, 1.0);
    }
)GLSL";

// ----------------------------------------------------------------
// 片元着色器：原图尺度找边缘，放大后尺度做柔化
// ----------------------------------------------------------------
static const char* kFrag = R"GLSL(
    in  vec2 vUV;
    out vec4 FragColor;

    uniform sampler2D uTex;
    uniform vec2  uSrcTexel;   // 1 / 原图尺寸  —— 定位边缘在哪
    uniform vec2  uDstTexel;   // 1 / 放大尺寸  —— 决定糊多宽
    uniform float uEdge;
    uniform float uInner;

    const vec3 LUMA = vec3(0.299, 0.587, 0.114);

    void main()
    {
        vec4 c = texture(uTex, vUV);

        // 在原图纹素尺度取 4 邻，做亮度 + 色度边缘检测
        vec4 sL = texture(uTex, vUV - vec2(uSrcTexel.x, 0.0));
        vec4 sR = texture(uTex, vUV + vec2(uSrcTexel.x, 0.0));
        vec4 sU = texture(uTex, vUV - vec2(0.0, uSrcTexel.y));
        vec4 sD = texture(uTex, vUV + vec2(0.0, uSrcTexel.y));

        float lc = dot(c.rgb, LUMA);
        float grad = max(max(abs(lc - dot(sL.rgb, LUMA)), abs(lc - dot(sR.rgb, LUMA))),
                         max(abs(lc - dot(sU.rgb, LUMA)), abs(lc - dot(sD.rgb, LUMA))));
        float chr = max(max(distance(c.rgb, sL.rgb), distance(c.rgb, sR.rgb)),
                        max(distance(c.rgb, sU.rgb), distance(c.rgb, sD.rgb)));

        // 平坦内部 -> 0，原始像素边界上 -> 1
        float edgeW = clamp(max(grad, chr * 0.5) * 6.0, 0.0, 1.0);

        // 在【放大后】像素尺度做 4-tap 柔化，过渡带约 1.2 个输出像素
        vec2 o = uDstTexel * 1.2;
        vec4 blur = (c * 4.0
                   + texture(uTex, vUV + vec2( o.x, 0.0))
                   + texture(uTex, vUV - vec2( o.x, 0.0))
                   + texture(uTex, vUV + vec2(0.0,  o.y))
                   + texture(uTex, vUV - vec2(0.0,  o.y))) / 8.0;

        // 边缘满强度羽化，内部按 uInner 轻微柔化
        FragColor = mix(c, blur, mix(uInner, 1.0, edgeW * uEdge));
    }
)GLSL";

// ----------------------------------------------------------------
static sf::Shader* gShader = nullptr;
static bool        gOK = false;

static void EnsureInit()
{
    if (gShader) return;

    // 本项目内置的 SFML 版本中 loadFromMemory 是实例方法、返回 bool，
    // 必须先建对象再加载；失败时立即释放，避免把无效着色器留在全局指针里。
    auto* sh = new sf::Shader();
    if (!sh->loadFromMemory(kVert, kFrag))
    {
        std::printf("[UpscaleAA] shader compile failed\n");
        delete sh;
        gOK = false;
        return;
    }
    gShader = sh;
    gOK = true;
}

bool AA::isAvailable() { EnsureInit(); return gOK; }

// ----------------------------------------------------------------
sf::Texture AA::UpscaleAA(const sf::Texture& src, float scale, float edge, float inner)
{
    sf::Texture empty;                       // 失败哨兵（0 尺寸），调用方用 getSize() 判
    if (scale <= 1.f)      return empty;

    // 该 SFML 版本没有 isValid()，空纹理的特征是尺寸为 0
    const sf::Vector2u ss = src.getSize();
    if (ss.x == 0 || ss.y == 0) return empty;

    EnsureInit();
    if (!gOK)              return empty;

    if (ss.x < 2 || ss.y < 2) return empty;

    const sf::Vector2u ds((unsigned)(ss.x * scale + 0.5f),
        (unsigned)(ss.y * scale + 0.5f));

    // 只改 GL 采样参数、不改像素，对 const 引用安全
    sf::Texture& srcMut = const_cast<sf::Texture&>(src);
    srcMut.setSmooth(true);                  // ★ 必须：否则采出来还是马赛克

    sf::RenderTexture rt;
    if (!rt.resize(ds))    return empty;     // SFML 3: create -> resize
    rt.setSmooth(true);

    gShader->setUniform("uTex", sf::Shader::CurrentTexture);
    gShader->setUniform("uSrcTexel", sf::Glsl::Vec2(1.f / ss.x, 1.f / ss.y));
    gShader->setUniform("uDstTexel", sf::Glsl::Vec2(1.f / ds.x, 1.f / ds.y));
    gShader->setUniform("uEdge", edge);
    gShader->setUniform("uInner", inner);

    sf::RectangleShape quad(sf::Vector2f((float)ds.x, (float)ds.y));
    quad.setTexture(&srcMut);
    quad.setPosition({ 0.f, 0.f });

    rt.clear(sf::Color::Transparent);
    rt.draw(quad, sf::RenderStates(gShader));
    rt.display();                            // ★ 忘了这行 = 全透明

    return rt.getTexture();                  // 按值返回，内部引用计数，极廉价
}

void AA::UpscaleAAInPlace(sf::Texture& t, float scale, float edge, float inner)
{
    sf::Texture r = UpscaleAA(t, scale, edge, inner);
    // 用尺寸判断是否放大成功：失败时返回的是 0 尺寸哨兵
    const sf::Vector2u sz = r.getSize();
    if (sz.x != 0 && sz.y != 0) t = std::move(r);
}

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
        // 每个文件一条独立的进度行：文件名 + 自己的轨道，互不干扰。
        // 轨道固定高度，填充条用 transform 缩放而非改 width，
        // 这样缓动动画由合成器执行，手机上不会因为布局重排而掉帧。
        // #bars 作为独立容器必须先占位：并发创建多行时，
        // 若它参与父级弹性布局被压缩，后加入的行会被挤成 0 高度而看不见，
        // 表现就是「只有一个文件有进度条」。这里显式固定每行高度并禁止收缩。
        "#bars{margin-top:6px;text-align:left;}"
        ".row{margin-top:10px;}"
        ".rname{font-size:12px;color:#aaa;text-align:left;white-space:nowrap;"
        "overflow:hidden;text-overflow:ellipsis;margin-bottom:4px;}"
        // 容器必须相对定位，前景滑动条才能以它为参照做绝对定位
        // min-height 与 height 同时写：部分手机浏览器会压缩空 block 的高度
         ".trk{position:relative;height:4px;min-height:4px;border-radius:2px;"
         "background:#333;overflow:hidden;display:block;}"
         // 确定进度填充条：z-index 必须低于 .ind，否则等待态的滑动前景条会被这条满格条盖住。
         // 这是「等待动画完全看不到」的直接原因——之前两者都在 z-index:auto 且 .ind 后插入，
         // 但 .ind 的父级宽度 100% 又常被误设为可见，导致满格条压在上面。
         ".fl{position:absolute;left:0;top:0;height:100%;width:100%;z-index:1;"
         // 用 scaleX 拉伸，因此 transform-origin 必须在左边，否则会从中间展开
         "transform:scaleX(0);transform-origin:0 50%;"
         "background:rgb(170,190,255);border-radius:2px;}"
         // 不确定进度（等待 MiuBarrd 处理数据）：前景线条固定长度，在轨道内从左向右循环滑动。
         // 刻意【不用】JS 的 requestAnimationFrame：
         //   并发 10 路时会有 10 个 rAF 自循环互相抢占，低端机很容易掉帧甚至被浏览器节流暂停，
         //   表现就是「完全没有等待动画」。改用 CSS @keyframes 由合成器驱动，
         //   多个元素各自独立播放，不依赖主线程，也不会因为 setInterval 更新文案而卡顿。
         // 关键修正：等待态必须把确定填充条隐藏（见 ind_()），否则满格条会完全遮住滑动条，
         // 用户只能看到一条静止的满格线，主观上就是「没有等待动画」。
         ".ind{position:absolute;left:0;top:0;height:100%;width:100%;display:none;z-index:2;}"
         // 前景条固定长度为轨道总长的 35%。
         // 【位移单位】translateX 的百分比基准是【元素自身宽度】，条宽 = 轨道 35%，
         // 因此轨道 100% 的长度 = 自身宽度的 100/35 ≈ 285.7%。换算表：
         //   位移 -100%   → 右端点位于轨道 0，整条完全在轨道左侧（只露出右端一角）
         //   位移    0%   → 左端点在轨道 0，整条完整可见（左端贴边）
         //   位移 285.7%  → 左端点到达轨道 100%，即「左边的点滑到最右端」
         //   位移 385.7%  → 整条完全滑出轨道右侧（比 285.7% 多出自身宽度）
         // 用户要求：起点是「右侧先露出」，终点是「左边的点抵达最右端后再滑出」。
         // 若终点只到 285.7%，左端点刚触到右边界动画就回到起点，
         // 观感上像「还没滑到底就被拽回去」；取 385.7% 才能让左端点完整走完全程。
         ".ind i{position:absolute;left:0;top:0;height:100%;width:35%;border-radius:2px;"
         "background:rgb(170,190,255);display:block;transform:translateX(-100%);}"
         // 只保留一套关键帧，缓动统一为 EaseOut（起步快、收尾慢）。
         // 之前用 indA(EaseInOut)/indB(EaseOut) 两套关键帧交替轮换，
         // 每轮切换都会产生一次速度突变，看着像「卡一下」，因此已移除轮换。
         "@keyframes indA{"
         "0%{transform:translateX(-100%);animation-timing-function:cubic-bezier(.33,1,.68,1);}"
         "100%{transform:translateX(385.7%);}"
         "}"
         // 【必须拆成独立属性】这里绝不能用 animation 简写：
         // 简写会在每次 class 变化时重置 animation-name，让正在播放的滑动条从头重启。
         ".ind.run i{animation-name:indA;animation-duration:1500ms;"
         "animation-timing-function:linear;animation-iteration-count:infinite;"
         "animation-fill-mode:both;}"
        "</style></head><body>"
        "<h2>\xE5\x90\x91MiuBarrd\xE4\xB8\x8A\xE4\xBC\xA0\xE5\x9B\xBE\xE7\x89\x87</h2>"
        "<div class='box'>"
        "<input id='f' type='file' accept='image/*' multiple>"
        "<label for='f'>\xE9\x80\x89\xE6\x8B\xA9\xE5\x9B\xBE\xE7\x89\x87</label>"
        // 进度条容器：每个文件由脚本动态追加一行 .row，不再有全局单条进度条
        "<div id='bars'></div>"
        "<div id='tip'></div><div id='list'></div>"
        "</div>"
        "<script>"
        "var f=document.getElementById('f');"
        "var tip=document.getElementById('tip');"
        "var list=document.getElementById('list');"
        "var bars=document.getElementById('bars');"
        // 【关键】all（本批文件总数）与 CNT（当前第几张/总数）必须在页面顶层声明。
        // 旧代码里 all 仅在 onchange 内裸赋值、CNT 仅在 send 内用 var 声明，
        // 而 makeBar 的 done() 却直接引用它们：取不到就抛 ReferenceError。
        // done() 中 settled/stopInd 已先执行、say() 抛错后异常向上冒泡，
        // 使 send 的 finish 半途中断，pump() 再也没机会推进 —— 表现为「传一张就停」。
        "var all=0;"
        "var CNT='0/0';"
        "function say(t,c){tip.style.color=c||'#9ad';tip.textContent=t;}"
        "function fmt(b){"
        "  if(b>=1048576)return (b/1048576).toFixed(1)+' MB/s';"
        "  return Math.max(0,Math.round(b/1024))+' KB/s';"
        "}"
        // ------------------------------------------------------------
        // 每个文件一条独立进度条
        // 并发上传时所有文件同时在传，若共用一条进度条，多个文件的字节数会互相覆盖，
        // 用户根本看不出每张图各自的进度。因此这里为每个文件创建一行 .row，
        // 各自持有独立的 DOM 引用、缓动动画帧与不确定态循环动画。
        // ------------------------------------------------------------
        // 创建一行进度条，返回该行的控制器对象：
        //   set(v)   —— 确定进度（0~1），内部 EaseOut 缓动
        //   ind()    —— 切到不确定态（等待 MiuBarrd 处理数据）
        //   reset()  —— 回到确定态
        //   done()   —— 完成态：拉满并停掉所有动画
        //   fail(t)  —— 失败态：红色并显示原因
        "function makeBar(label){"
        "  var row=document.createElement('div');row.className='row';"
        "  var nm=document.createElement('div');nm.className='rname';nm.textContent=label;"
        "  var trk=document.createElement('div');trk.className='trk';"
        "  var fl=document.createElement('div');fl.className='fl';"
        "  var ind=document.createElement('div');ind.className='ind';"
        "  ind.appendChild(document.createElement('i'));"
        "  trk.appendChild(fl);trk.appendChild(ind);"
        // 轨道行一建出来就立刻挂到 #bars 上（此时填充条 scaleX(0) 不可见，只有空轨道），
        // 这样并发 10 个文件时用户能马上看到 10 条轨道。
        // 若等到某张真正开始传输才创建行，观感上就像「只有一个文件有进度条」。
        "  row.appendChild(nm);row.appendChild(trk);"
        "  bars.appendChild(row);"
        // 每行独立的状态：当前显示值、确定态动画帧、是否处于不确定态
        // 不确定态已改为纯 CSS 动画驱动，这里不再需要 rAF 循环句柄。
        "  var val=0,anim=0,isInd=false;"
        // 标记当前不确定态是「占位等待」（压缩中，随时可能退场）还是真实等待服务端。
        // 占位态收到第一个真实百分比就必须立刻退出，否则 isInd 会一直把进度挡回去，
        // 该行就会永久停在滑动条上，永远显示不出传输进度。
        "  var isPlaceholder=false;"
        "  function set(v){"
        // 进入真实的确定态：若当前还停在占位等待态（压缩窗口期），先把等待动画收掉，
        // 否则 isInd 会一直为 true，后续所有百分比都被忽略，该行永远停在滑动条上。
        "    if(isInd&&isPlaceholder){reset();}"
        "    if(isInd)return;"              // 真正的等待 MiuBarrd 处理期间忽略具体百分比
        "    if(v>1)v=1;"
        "    if(v<0)v=0;"
        "    var from=val,to=v,t0=0,dur=260;"
        "    if(to<=from){val=to;fl.style.transform='scaleX('+to+')';return;}"
        "    if(anim){cancelAnimationFrame(anim);anim=0;}"
        "    function step(ts){"
        "      if(!t0)t0=ts;"
        "      var t=(ts-t0)/dur;"
        "      if(t>1)t=1;"
        // EaseOutCubic：起步快、收尾慢，进度变化看起来更自然
        "      var e=1-Math.pow(1-t,3);"
        "      val=from+(to-from)*e;"
        "      fl.style.transform='scaleX('+val+')';"
        "      if(t<1)anim=requestAnimationFrame(step);"
        "      else{anim=0;val=to;}"
        "    }"
        "    anim=requestAnimationFrame(step);"
        "  }"
        // 等待态最少展示时长（毫秒）：
        // 服务端几乎立刻回响应时，等待态可能只存在几十毫秒，用户完全看不到那条滑动
        // 指示条。这里保留一个很小的下限让动画「能被感知」，但不能用 900ms 这种量级——
        // 那是纯粹的人为等待，哪怕只传一张小图也要白白多等近一秒。
        "  var IND_MIN_MS=350;"
        "  var indSince=0;"
        "  function ind_(){"
        // 已收尾（done / fail）的行绝不能再回到等待态。
        // 这是「两张图都已被接受，但先传完那张仍在播放等待动画」的根本修复：
        // 收尾是异步回调，而 500ms 定时器的 tick 可能在收尾之后仍执行一次 bar.ind()，
        // 一旦进入等待态就再没有收尾机会，该行会永久停在滑动条上。
        "    if(settled)return;"
        "    if(isInd)return;"
        "    isInd=true;"
        "    indSince=(new Date()).getTime();"
        "    if(anim){cancelAnimationFrame(anim);anim=0;}"
         // 进入等待态时把确定填充条隐藏。
         // 之前这里保留 scaleX(1) + opacity:1，满格的填充条会把 35% 宽的同色滑动条
         // 完全盖在下面，用户看到的是一条静止满格线——这就是「等待动画看不到」的根因。
         // 隐藏后轨道上只剩滑动前景条，动势才清晰可辨。
         // 注意：隐藏的是填充条而不是整行，每行仍是独立 DOM，不会影响其它文件的进度显示。
         "    fl.style.opacity='0';"
        // 用 class 启动 CSS 动画而不是靠自己算位移：
        // display:none 时动画不参与计时，切到 block + run 同一帧即可开始播放，
        // 多行并发各自独立播放，不会因为主线程被 setInterval 文案刷新占用而停住。
        "    ind.style.display='block';"
        // 【顺序极其关键】先把 run 摘掉再挂回。
        // 若只 add 不先 remove，浏览器会认为 class 没变化而沿用上一轮的动画；
        // 强制走一次「停动画 → 启动动画」，CSS 动画引擎才会从 translateX(-100%) 重新计时。
        "    ind.classList.remove('run');"
        "    ind.classList.add('run');"
        "  }"
        // 等待态是否已展示足够久；不足时把收尾推迟到满足最短时长之后再执行，
        // 这样「传输极快」的场景也能看到完整的滑动动画，而不是一闪而过。
        "  function indHold(cb){"
        "    if(!isInd){cb();return;}"
        "    var left=IND_MIN_MS-((new Date()).getTime()-indSince);"
        "    if(left<=0){cb();return;}"
        "    setTimeout(cb,left);"
        "  }"
        // 占位等待态：用于「已建好行但业务还没开始」的窗口期（如正在本地压缩）。
        // 与 ind() 的区别是不设最短展示时长承诺，压缩一结束就立刻切回真实进度。
        "  function wait_(){isPlaceholder=true;ind_();}"
         "  function reset(){"
         "    isInd=false;"
         // 先摘掉 run 再隐藏：否则下次 display:block 时保留的是播放到一半的动画，
         // 指示条会从上次中断的位置突然出现，看起来像闪了一下。
         "    ind.classList.remove('run');"
         "    ind.style.display='none';"
         "    fl.style.opacity='1';"
         "  }"
         // 收尾加锁：done/fail 一旦触发就把 settled 置位，
         // 之后任何迟到的 tick / checkSent 都不能再把该行拽回等待态。
         // 这正是「两张图都已被接受，但先传完那张仍在播放等待动画」的原因：
         // 服务端已回响应时 finished 才置 1，而 500ms 定时器可能在 finish 之前
         // 又执行了一次 tick → bar.ind()，若此时 indHold 的延迟回调还没跑，
         // 该行就会一直停在滑动条上。用 settled 提前拦截，不再依赖时序。
         "  var settled=false;"
         // 标记本行是否为「整批最后完成」的那一行：
         // 只有它需要在收尾时真正停掉加载动画，其余已传完的行继续保留滑动等待效果。
         "  var isLast=false;"
         // 统一的收尾动作：无论 done 还是 fail 都先走这里，保证「任何路径都能停掉滑动动画」。
         // 只靠 reset() 不够——reset() 里摘 run 后若同一帧又被 add 回来（tick 迟到），
         // 该行会重新开始滑动；因此这里额外把 display 置 none 并内联 animation:none，
         // 用行内样式压过 CSS 类，彻底杜绝残留的加载中动画。
         // 【关键补充】动画实际跑在 .ind 的子元素 <i> 上（.ind.run i{animation-name:indA}），
         // 只把父元素 display 置 none 在部分手机浏览器里不会真的取消子元素的动画计时，
         // 再次显示时仍可能从旧相位继续跑；因此必须把子元素的 animation 也内联禁掉。
         "  function stopInd(){"          // 停掉不确定态的一切视觉残留
         "    isInd=false;isPlaceholder=false;"
         "    ind.classList.remove('run');"
         "    ind.style.display='none';"
         "    ind.style.animation='none';"   // 行内 !important 语义：直接禁掉动画
         // 真正的滑动条是 <i>，必须单独把它的动画也停掉，否则换行后会接着滑
         "    var ii=ind.firstChild;"
         "    if(ii&&ii.style){ii.style.animation='none';}"
         "    fl.style.opacity='1';"
         "  }"
         // 收尾：每一行都必须停掉自己的加载动画。
         // 早先的实现只在「整批最后完成」的那一行调 stopInd()，本意是让其余行
         // 保留滑动等待效果，但并发 10 张时这是明确的视觉 bug：先传完的 9 行会永久
         // 停在滑动条上，用户看到的就是「只有最后一个进度条结束了加载动画」。
         "  function done(){"
         // 幂等：finish 可能被 onload 与 onerror 先后触发一次以上，
         // 重复执行会把已收尾的行再改一遍，也会让 settled 之后的判断混乱。
         "    if(settled)return;"
         "    settled=true;"
         "    stopInd();"
         "    if(anim){cancelAnimationFrame(anim);anim=0;}"
         // 收尾时把填充条重新显示并拉满：等待态期间它被隐藏，这里必须恢复，
         // 否则完成的文件会显示成空轨道。
         "    val=1;fl.style.transform='scaleX(1)';fl.style.opacity='1';"
         // 【关键】同时把顶层汇总文案从「等待 MiuBarrd 处理数据」改回进度：
         // tick 在等待态每 150ms 写一次该文案，若不在收尾时夺回文案控制权，
         // 即使该行动画已停，顶部仍会固执地显示「等待处理」，看起来就像没结束。
         "    say('100%  '+CNT,'#9ad');"
         "  }"
         // 标记本行为整批最后一行。若该行已经收尾（done 抢先执行），
         // 这里立即补一次 stopInd，弥「先收尾后标记」的时序差异。
         "  function markLast(){"
         "    isLast=true;"
         "    if(settled){stopInd();}"
         "  }"
         "  function fail(msg){"
         "    if(settled)return;"          // 与 done 同理，失败收尾同样只执行一次
         "    settled=true;"
         "    stopInd();"
         "    if(anim){cancelAnimationFrame(anim);anim=0;}"
         "    val=1;fl.style.transform='scaleX(1)';fl.style.opacity='1';"
         "    fl.style.background='#e88';"
         "    if(msg)nm.textContent=label+'  —  '+msg;"
         "  }"
        "  return {set:set,ind:ind_,wait:wait_,reset:reset,done:done,fail:fail,markLast:markLast};"
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
        // 传输前不再做任何压缩 / 重编码：原图字节直接上传。
        // 旧实现先用 canvas 等比缩到 1920 长边再以 JPEG 0.82 重编码，
        // 手机端单张就要走「解码整张大图 -> 降采样绘制 -> JPEG 重编码」三步，
        // 中低端机上是几百毫秒到两秒的纯等待，而这正好卡在「选完图到真正开始传」之间，
        // 用户感知就是发出去慢、后续图片迟迟不启动。去掉后选完图立即进入传输，
        // 首字节时间只受网络上行影响，画质也不再受二次有损编码损失。
        // 上传单个文件；bar 为该文件专属的进度条控制器，idx/all 仅用于文案序号
        "function send(file,bar,idx,done){" 
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
        // 传输完成前上限不再压到 99：body 全部发完后进度直接到 100，
        // 之后的服务端处理改用「校验中」文案提示，避免 99% 长时间静止。
        "    var cap=100;"
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
        // 字节已发完但服务端尚未回响应：明确提示正在等待 MiuBarrd 处理数据，
        // 让用户知道卡住的是 PC 端处理而不是网络，且不会误以为上传失败。
        "    if(!finished&&base>0&&sent>=base){"
        // 此处已无「百分比」可言，进度条切换成固定长度前景线条的循环流动，
        // 继续用确定进度会让用户误以为卡死，循环条才能表达「仍在进行」。
        "      bar.ind();"
        // 每行有自己的文件名标题，顶部汇总行只描述整体进度与等待状态
        "      say('\xE7\xAD\x89\xE5\xBE\x85MiuBarrd\xE5\xA4\x84\xE7\x90\x86\xE6\x95\xB0\xE6\x8D\xAE  '+CNT,'#9ad');"
        "      return;"
        "    }"
        // 传输中：用已发送字节占比驱动确定进度条（barSet 内部再做 EaseOut 缓动）
        "    bar.set(base>0?(sent/base):0);"
        // 速度后面追加「当前第几张/总数」，让用户知道整体进度而非单张进度
        "    say(shown.toFixed(0)+'%  '+fmt(sent/sec)+'  '+CNT,'#9ad');"
        "  }"
        // onprogress 的最后一帧往往只比 onload 早极短时间，仅靠 500ms 定时器轮询
        // 很可能一次都轮不到「字节已发完」这个中间状态，等待动画就永远不会启动。
        // 因此这里直接判断：字节发完就立刻切等待态，不依赖定时器。
        "  function checkSent(){"
        "    var base=total>0?total:((file.size||0)+MIME_PAD);"
        "    if(!finished&&base>0&&sent>=base)bar.ind();"
        "  }"
        // 同一批上传共享的总数，由 onchange 在每次开始时赋值
        "  var CNT=(idx+1)+'/'+all;"
        "  xhr.open('POST','/upload?name='+encodeURIComponent(file.name||'image.jpg'));"
        "  xhr.upload.onprogress=function(e){" 
        "    sent=e.loaded||0;"
        "    if(e.lengthComputable&&e.total>0)total=e.total;"
        "    checkSent();"
        "  };"
        // 部分浏览器在请求体全部写入 socket 后不再回调 onprogress，
        // upload.onload 是「请求体已发送完毕」的可靠信号，用它兜底启动等待动画。
        "  xhr.upload.onload=function(){checkSent();};"
        // 定时器同时负责刷文案与「字节已发完 → 切等待态」的判定。
        // 周期取 150ms 而非 500ms：500ms 会让等待动画最迟晚半秒才出现，
        // 主观上就是「反应很慢」；而这里只做几次字符串拼接，开销可忽略。
        // 【必须分行】旧代码把 setInterval 与下面的 function finish 写在同一行，
        // 少了行尾逗号后两个字符串直接拼接，页面脚本报 SyntaxError，
        // 整段 JS 一行都不执行——进度条动画自然「怎么改都不生效」。
        "  timer=setInterval(tick,150);"
        // 收尾必须幂等：xhr 的 onload / onerror / onabort 中可能有两条先后触发，
        // 重复调用 done(err) 会让串行上传的 done++ 多算一次，使 done>=all 提前成立，
        // 剩余图片被直接跳过——表现同样是「传一张就停」。
        "  var ended=false;"
        "  function finish(err){"
        "    if(ended)return;"
        "    ended=true;"
        "    if(timer){clearInterval(timer);timer=0;}"
        // 服务端已确认入队：此时才把进度定格到 100%，避免传输中提前显示完成
        "    if(!err){"
        "      finished=1;"
        "      shown=100;"
        // 计算平均速度需要发送体长度，这里按与 tick 相同的口径取分母
        "      var len=total>0?total:((file.size||0)+MIME_PAD);"
        "      var sec=((new Date()).getTime()-t0)/1000;"
        "      if(sec<0.1)sec=0.1;"
        // 该文件自己的进度条拉满收尾，顶部汇总继续显示整批进度
        "      bar.done();"
        "      say('100%  '+fmt(len/sec)+'  '+CNT,'#9ad');"
        "    }"
        "    else{bar.fail(err);}"
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
        // 全部上传结束后向服务端二次确认：把 PC 端真实队列数量回显给用户，
        // 避免「手机显示成功但电脑端没图」的假成功。失败时按部分失败提示。
        // 注意：这里必须定义在 onchange 之前——onchange 内部会调用它。
        "function confirmPending(bad){"
        "  fetch('/pending').then(function(r){return r.json();})"
        "    .then(function(j){"
        // 服务端 /pending 返回的是 {"pending":N,"items":[...]}，取 pending 字段
        "      var n=(j&&typeof j.pending==='number')?j.pending:0;"
        "      if(bad>0){"
        // 只报完成 / 失败数量，不再显示 PC 端待取张数：
        // 队列是否被消费属于 PC 端内部状态，手机用户看到「待取 N 张」既无法操作也容易误解为未成功。
        "        say('\xE5\xAE\x8C\xE6\x88\x90\xEF\xBC\x8C'+bad+' \xE4\xB8\xAA\xE5\xA4\xB1\xE8\xB4\xA5  '+all+'/'+all,'#e88');"
        "      }"
        "      else{"
        "        say('100%  \xE5\xB7\xB2\xE5\xAE\x8C\xE6\x88\x90  '+all+'/'+all,'#ad9');"
        "      }"
        // 各文件自己的进度条已在 send 完成/失败时收尾，这里只更新顶部汇总文案，
        // 不再有全局进度条需要复位。
        "    })"
        "    .catch(function(){"
        "      say(bad>0?('\xE5\xAE\x8C\xE6\x88\x90\xEF\xBC\x8C'+bad+' \xE4\xB8\xAA\xE5\xA4\xB1\xE8\xB4\xA5  '+all+'/'+all):('100%  \xE5\xB7\xB2\xE5\xAE\x8C\xE6\x88\x90  '+all+'/'+all),'#ad9');"
        "    });"
        "}"
        "f.onchange=function(){"
        "  var files=Array.prototype.slice.call(f.files||[]);"
        "  if(!files.length)return;"
        "  var i=0,bad=0,live=0,done=0;"
        // 清掉上一批的进度行，避免多轮上传后列表无限变长。
        // 必须用 removeChild 逐个摘除而不是 innerHTML=''：
        // 直接清空会让上一批仍在跑 xhr 的行失去父节点，但它们的 setInterval 还活着，
        // 继续对已脱离文档的节点写样式，既不释放也没意义；
        // 逐个移除时能顺带把每行的动画停掉，新批次的行才有干净的绘制环境。
        "  while(bars.firstChild)bars.removeChild(bars.firstChild);"
        // 总数固定为本次选中的文件数；all 供 send 内部拼「当前/总数」
        "  all=files.length;"
        // 单张串行上传：同一时刻只允许一张在传，上一张彻底结束后才发下一张。
        // 早先是 10 路并发，但手机端上行带宽与 PC 端解码能力都有限，
        // 10 路同时压缩 + 服务端同时解析会让每张都变慢，也更容易整体超时；
        // 串行反而让单张更快完成，进度也更好理解（永远只有一行在动）。
        // live 继续作为「在传张数」使用，上限恒为 1，因此原有的 live-- 与
        // done>=all 的收尾判定都不需要改动。
        "  function pump(){"
        "    while(live<1&&i<files.length){"
        "      var myIdx=i;"
        "      var file=files[i++];"
        "      live++;"
        // 每个文件单独建一条进度条，标题带上序号与文件名，便于区分是哪一张。
        "      var myBar=makeBar('#'+(myIdx+1)+'  '+(file.name||'image.jpg'));"
        // 已彻底去掉压缩环节，行建好即可直接发送，无需再进入「等待压缩」的占位态。
        "      say('0%  0 KB/s  '+done+'/'+all,'#9ad');"
        // 原文件直接上传：服务端收完全部字节后才解析入队，因此回调返回时对方队列已就绪，
        // 下一张可立刻开始，不再有本地解码 / 重编码的预处理等待。
        // 完成回调同样只能执行一次：重复进入会让 done++ / live-- 被多算，
        // 串行队列提前认为「整批已结束」，后面排队的图就永远发不出去。
        "      var settled=false;"
        "      send(file,myBar,myIdx,function(err){"
        "        if(settled)return;"
        "        settled=true;"
        "        if(err)bad++;"
        "        done++;live--;"
        // 本批全部结束：myBar.done() 已在 send 的 finish 里执行过，
        // 这里只需收尾 f.value 并做服务端二次确认；
        // 各行动画已由 done() 统一停掉，不再需要 markLast 区分「最后一行」。
        "        if(done>=all){"
        "          f.value='';confirmPending(bad);return;"
        "        }"
        "        pump();"     // 上一张已结束，接着发下一张（串行）
        "      });"
        "    }"
        "  }"
        "  pump();"
        "};"
        "</script></body></html>";

    /// <summary>
    /// 同时处理的 HTTP 连接数上限。
    /// 手机端网页已改为单张串行上传，正常情况下同一时刻只有 1 条上传连接；
    /// 取 16 用于容纳浏览器预连接、重复打开页面以及其他静态资源请求等偶发并发，
    /// 同时避免异常情况下无限开线程耗尽句柄。
    /// </summary>
    constexpr int kMaxConcurrentClients = 16;

    /// 统计当前活跃的客户端连接数，配合下面的信号量做限流
    std::atomic<int> g_activeClients{ 0 };

    /// 全局停止标志：由 runServer 在停止时置位，供 acquireClientSlot 中断等待。
    /// 匿名命名空间内的自由函数无法访问 ShareImage 的成员，故用该标志传递状态。
    std::atomic<bool> g_serverStopping{ false };

    /// 连接槽同步原语：release 时立即唤醒正在等待的 accept 循环。
    /// 用条件变量取代原来的 1ms 轮询，槽位释放后等待方在微秒级被唤醒，
    /// 彻底消除「槽位空窗期」带来的固定延迟（高并发下也省掉自旋的 CPU 占用）。
    std::mutex              g_slotMutex;
    std::condition_variable g_slotCv;

    /// AWS：阻塞直到有空闲连接槽；服务停止时立即返回 false
    bool acquireClientSlot()
    {
        std::unique_lock<std::mutex> lk(g_slotMutex);

        // 谓词里同时检查停止标志，停止时 notify_all 能立刻打断等待，
        // 保证 stop() 里的 join() 不会因为连接占满而永久阻塞。
        g_slotCv.wait(lk, [] {
            return g_serverStopping.load()
                || g_activeClients.load() < kMaxConcurrentClients;
        });

        if (g_serverStopping.load())
            return false;

        g_activeClients.fetch_add(1);
        return true;
    }

    /// 释放一个连接槽，并唤醒可能在等待槽位的 accept 循环
    void releaseClientSlot()
    {
        {
            std::lock_guard<std::mutex> lk(g_slotMutex);
            g_activeClients.fetch_sub(1);
        }
        // 必须在释放锁之后再 notify，避免被唤醒线程立刻又阻塞在锁上（惊群式抖动）
        g_slotCv.notify_one();
    }

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
    /// 接收端图像后处理：抗锯齿放大，并把无损格式重编码为 JPEG 以减小队列内存。
    /// 任何一步失败都必须保持 img.data 原样，绝不写入空字节——否则调用方会收到「空图」。
    /// </summary>
    /// <param name="img">已上传的图片对象，函数会就地替换其字节、文件名与 MIME。</param>
    void upscaleReceived(SharedImage& img)
    {
        if (img.data.empty()) return;

        // SFML 解码失败（HEIC 等不支持的格式）时保留原始字节，
        // 保证「至少能收到图」，解码交给调用方自行处理。
        sf::Image srcImg;
        if (!srcImg.loadFromMemory(img.data.data(), img.data.size())) return;

        // ------------------------------------------------------------
        // 关键：handleUpload 运行在 detached 网络线程，该线程没有 GL 上下文。
        // sf::Texture / RenderTexture / Shader 全部依赖当前线程的 GL 上下文，
        // 在此处调用会失败或取回 0 尺寸图像，最终把 img.data 覆盖成空。
        // 因此这里【只做无 GL 依赖的重编码】：用 sf::Image 完成解码与再编码，
        // 抗锯齿放大交给持有 GL 上下文的主线程（调用方在绘制时执行 AA）。
        // ------------------------------------------------------------
        const sf::Vector2u sz = srcImg.getSize();
        if (sz.x == 0 || sz.y == 0) return;

        // 无损格式（PNG / BMP / GIF）多为截图或线稿，重编码为 JPEG 体积可降一个数量级，
        // 队列内存与主线程后续 AA 放大的开销都随之下降。
        // 该版本 saveToMemory 只接受 format 参数并返回 optional 缓冲区。
        if (isLossyMime(img.mimeType)) return;

        auto encoded = srcImg.saveToMemory("jpg");
        // 编码失败或结果反而更大时保留原始字节，避免做负优化或产生空图
        if (!encoded || encoded->empty() || encoded->size() >= img.data.size()) return;

        img.data = std::move(*encoded);
        // 名称与类型必须同步改写，否则调用方按原名后缀选解码器会失败
        img.mimeType = "image/jpeg";
        const size_t dot = img.name.find_last_of('.');
        img.name = (dot == std::string::npos ? img.name : img.name.substr(0, dot)) + ".jpg";
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
    // 置位全局停止标志：若 server 线程正卡在 acquireClientSlot 的等待里，
    // 只有它能唤醒该自旋，否则 join() 会永久阻塞，表现为「停止服务后界面卡死」。
    g_serverStopping = true;

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

    // backlog 必须覆盖 10 路并发：手机端一次性建 10 条连接，
    // 若排队长度只有 8，多出的 SYN 会被系统丢弃并触发客户端重传，
    // 在手机上表现为「偶尔连不上 / 要刷新好几次才打开页面」。
    // 这里取 64，远大于并发度，留足重传与突发建连的余量。
    if (listen(serverSocket_, 64) == SOCKET_ERROR)
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
    // 先清掉上一轮的停止标志，再启动线程：顺序颠倒会让新线程一进 runServer
    // 就被 acquireClientSlot 判定为「正在停止」而立刻退出。
    g_serverStopping = false;
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

            // 区分「监听套接字已被关闭」与「瞬时错误」：
            // 前者直接退出，不必陪跑一次 10ms 休眠；只有真正的瞬时错误才退避。
            if (serverSocket_ == INVALID_SOCKET)
                break;

            // accept 失败通常是套接字已被关闭，短暂休眠避免空转占满 CPU
            SLEEP_MS(5);
            continue;
        }

        // 先占一个连接槽再开绪：超过上限时阻塞在 accept 循环里，
        // 而不是无限制地创建分离线程，避免并发上传时句柄 / 内存暴涨。
        if (!acquireClientSlot() || !running_)
        {
            releaseClientSlot();
            closesocket(cs);
            break;
        }

        // 每个连接一个线程：上传大图耗时可能较长，不能阻塞其它请求。
        // 线程结束时必须归还连接槽，否则并发位会被永久占满。
        std::thread([this, cs]()
            {
                handleClient(cs);
                releaseClientSlot();
            }).detach();
    }

    std::cout << "[ShareImage] server stopped" << std::endl;
}

// ================= HTTP 请求分发 =================

void ShareImage::handleClient(SOCKET s)
{
    // 设置收发超时：手机端建连后中途断开（切后台 / 锁屏）时 recv 不再永久阻塞，
    // 否则每个断开的客户端都卡一个永久阻塞的分离线程，句柄与内存持续泄漏。
#ifdef _WIN32
    // 接收超时从 15000ms 收紧到 8000ms：
    // 正常上传每块数据都在毫秒级到达，只有客户端真实断链（锁屏 / 切后台 / 弱网掉线）
    // 才会真的撞上超时；缩短后该连接占用的连接槽能更快归还，
    // 下一张图不必再等一个已经死掉的连接释放位置，主观上就是「处理下一张更快」。
    DWORD recvTimeout = 8000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&recvTimeout), sizeof(recvTimeout));

    // 发送超时同样必要：手机端提前关闭连接时 send 否则会长时间阻塞
    // 发送超时保持 15000ms：响应体只有几十字节，慢的不是发送而是客户端接收，
    // 缩短它只会在手机端网络抖动时把「已成功入队」误报成失败，因此不动。
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
        // 一次预分配到位：避免逐次 insert 触发 1.5 倍扩容 + 全量 memcpy，
        // 2MB 图片在旧逻辑下会经历十来次重新分配，是「等待处理」的隐性开销。
        if (clen > body.size())
        {
            // +64KB 余量：客户端常会多发尾部字节（分隔符 / keep-alive 管道），
            // 预留后这些多余字节直接落进已分配的容量里，不再触发扩容。
            body.reserve(clen + 64 * 1024);
        }

        while (body.size() < clen && running_)
        {
            // 使用 256KB 线程局部缓冲：分离线程栈默认仅 1MB，
            // 放大到栈上会拉高溢出风险；thread_local 只在每个连接线程首次使用时分配一次。
            static thread_local uint8_t bodyBuf[kRecvBufferSize];
            // 按「还差多少」限长，避免为剩余几十字节也去尝试超大 recv
            const size_t want = std::min<size_t>(clen - body.size(), sizeof(bodyBuf));
            int n = recv(s, (char*)bodyBuf, (int)want, 0);
            if (n <= 0) break;
            body.insert(body.end(), bodyBuf, bodyBuf + n);
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
            // 与 Content-Length 分支各自持有线程局部缓冲，均在首次使用时分配，互不干扰
            static thread_local uint8_t chunkBuf[kRecvBufferSize];
            int n = recv(s, (char*)chunkBuf, (int)sizeof(chunkBuf), 0);
            if (n <= 0) break;
            body.insert(body.end(), chunkBuf, chunkBuf + n);
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
        // 用 std::move 把 body 的所有权交给 handleUpload，避免整段图片字节再拷一次。
        // 10 路并发上传 2MB 级图片时，这次多余的拷贝既浪费时间又制造内存峰值，
        // 是手机端「等待处理数据」被拖长的原因之一。body 在此调用后不再使用。
        handleUpload(s, std::move(body), ctype, queryParam(query, "name"));
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

void ShareImage::handleUpload(SOCKET s, std::vector<uint8_t>&& body,
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
            // 传右值：parseMultipart 直接接管 body 存储，图片字节不再多拷一份
            img = parseMultipart(std::move(body), boundary);

        // multipart 解析失败时退回「整段 body 当图片」，避免明明收到图却因边界识别问题被丢弃。
        // 这里必须用 std::move：body 是以右值引用接管的独占数据，
        // 拷贝一次就多出一整份 2MB 级内存与一次 memcpy，直接体现在手机端「等待处理」的时长上。
        if (!img.valid && looksLikeImage(body))
        {
            img.data = std::move(body);
            img.valid = true;
        }
    }
    else
    {
        // 裸 body 上传（旧页面 / 第三方客户端）：整段字节就是图片
        img.data = std::move(body);
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

    // 先写入原始文件名与 MIME：upscaleReceived 需要据此判断原始格式是否为有损格式
    if (img.valid)
    {
        img.name = name;
        img.mimeType = partType;
    }

    // 序号在成员函数中统一分配，避免 static 解析函数触碰非静态成员
    if (img.valid) img.seq = ++seqGen_;

    if (img.valid)
    {
        // move 之前先快照日志所需字段：入队后 img 已是 moved-from 状态，
        // 再读 img.data.size() / img.name 只会得到空值。
        const uint32_t    queuedSeq = img.seq;
        const size_t      queuedBytes = img.data.size();
        const std::string queuedName = img.name;
        const std::string queuedType = img.mimeType;

        size_t pending = 0;
        {
            std::lock_guard<std::mutex> lk(uploadMutex_);
            // 用 std::move 入队：旧实现 push(img) 会把整张图再深拷一份，
            // 单张 8MB 原图就多一次完整 memcpy 与一倍内存峰值，
            // 而持锁期间主线程的 hasUploadedImage() / popAllUploadedImages()
            // 也被一起堵住，直接影响「图推到等待区后主线程多久能拿到」。
            uploadQueue_.push(std::move(img));
            pending = uploadQueue_.size();
        }

        // 累计计数独立于队列：队列被 PC 端取空后该值仍然保留，
        // 供 getReceivedImageCount() 给界面显示「本次共收到多少张」。
        const int receivedNow = ++receivedCount_;

        // 注意：img 已在上面被 move 进队列，这里不能再读 img.data.size() / img.name，
        // 否则拿到的是 moved-from 的空值；所需字段在 move 之前先快照下来。
        // 入队日志刻意打印 seq / 队列长度 / 文件名 / 类型，
        // 手机端显示成功后，可在 PC 控制台直接确认「图确实进了队列」。
        std::cout << "[ShareImage] image queued (seq=" << queuedSeq
            << ", pending=" << pending
            << ", bytes=" << queuedBytes
            << ", received=" << receivedNow
            << ", name=" << (queuedName.empty() ? "(none)" : queuedName)
            << ", type=" << (queuedType.empty() ? "(unknown)" : queuedType)
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
    // 头部通常只有几百字节，先预留 8KB，覆盖绝大多数带查询串的上传请求，
    // 避免每次 recv 后都触发一次容量翻倍。
    raw.reserve(8 * 1024);
    static thread_local char hdrBuf[kRecvBufferSize];

    // 先读到头部结束标记 \r\n\r\n，body 可能随头部一起到达
    while (true)
    {
        // 用 256KB 缓冲一次收更多数据：手机端上传时头部与首段 body 常在同一
        // TCP 段内到达，缓冲够大就能少一次 recv 往返，首字节延迟明显降低。
        int n = recv(s, hdrBuf, (int)sizeof(hdrBuf), 0);
        if (n <= 0) return raw;

        raw.insert(raw.end(), hdrBuf, hdrBuf + n);

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

SharedImage ShareImage::parseMultipart(std::vector<uint8_t>&& body,
    const std::string& boundary)
{
    SharedImage img;
    if (boundary.empty()) return img;

    std::string delim = "--" + boundary;

    // 【性能关键】这里绝不能再把 body 整段拷进 std::string。
    // 旧实现 "std::string data((const char*)body.data(), body.size())" 会对每张图
    // 做一次全量内存拷贝；10 路并发上传 2MB 级别的图时，瞬间产生几十 MB 的
    // 临时分配 + 一次 memcpy，是手机端「等待处理数据」被拖长的直接原因之一。
    // 改成直接在 body（vector）上做有序查找，零额外分配：
    //   findSeq  —— 从 from 位置找 delim 首次出现处
    //   sub      —— 取 [b, e) 区间的 string（仅用于极小的 part 头部）
    auto findSeq = [&body](const std::string& needle, size_t from) -> size_t
    {
        if (needle.empty() || from >= body.size()) return std::string::npos;
        auto it = std::search(body.begin() + from, body.end(),
            needle.begin(), needle.end());
        return (it == body.end()) ? std::string::npos
                                  : static_cast<size_t>(it - body.begin());
    };

    size_t pos = findSeq(delim, 0);
    if (pos == std::string::npos) return img;

    // 逐个 part 扫描，取第一个带文件内容（存在 filename=）的部分
    while (true)
    {
        size_t partStart = findSeq("\r\n\r\n", pos);
        if (partStart == std::string::npos) break;

        size_t contentStart = partStart + 4;
        size_t nextDelim = findSeq(delim, contentStart);
        if (nextDelim == std::string::npos) break;

        // part 头部很小（通常 < 200 字节），这里才允许构造临时 string
        const std::string headers((const char*)body.data() + pos, partStart - pos);
        bool isFile = headers.find("filename=") != std::string::npos;

        if (isFile)
        {
            // part 内容末尾的 \r\n 属于分隔符的一部分，需去掉
            size_t contentEnd = nextDelim;
            if (contentEnd >= 2 && body[contentEnd - 2] == '\r' && body[contentEnd - 1] == '\n')
                contentEnd -= 2;

            if (contentEnd > contentStart)
            {
                // 这里【不能】再 assign 拷贝：body 已不会再被使用，
                // 单张 8MB 原图拷贝一次就是一次完整 memcpy（约几毫秒）加一倍内存峰值，
                // 而这段耗时完全落在「传输完成 → 图进队列」的等待里。
                // 因此优先把整段 body 移动出来，再就地剪掉首尾的多余字节：
                // resize 只会移动尾部数据并改长度，不重新分配，成本远低于全量拷贝。
                if (contentStart == 0 && contentEnd == body.size())
                {
                    img.data = std::move(body);
                }
                else
                {
                    img.data = std::move(body);
                    img.data.erase(img.data.begin() + contentEnd, img.data.end());
                    img.data.erase(img.data.begin(), img.data.begin() + contentStart);
                }
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

    // 等待 cloudflared 打印公网地址。
    constexpr int  kTunnelPollMinMs = 2;    // 首次轮询间隔（首字节最快到达）
    constexpr int  kTunnelPollMaxMs = 4;   // 轮询间隔上限，兼顾 CPU 占用
    constexpr DWORD kTunnelWaitTotalMs = 15000;

    DWORD waitedMs = 0;
    int   pollMs = kTunnelPollMinMs;

    while (waitedMs < kTunnelWaitTotalMs)
    {
        SLEEP_MS(pollMs);
        waitedMs += pollMs;

        std::string output = readTunnelOutput();
        if (!output.empty())
        {
            // 一旦读到输出（含地址），后续无需再退避，缩短下一次轮询
            pollMs = kTunnelPollMinMs;

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
        else
        {
            // 无新数据时才逐步退避，避免把 CPU 浪费在空轮询上
            pollMs = min(pollMs * 2, kTunnelPollMaxMs);
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

        // 关闭隧道无需完整等 1 秒：进程通常在几十毫秒内退出，
        // 这里以 10ms 为步长最多等 300ms，超时再强杀，关闭体验明显更快。
        for (int i = 0; i < 30; ++i)
        {
            DWORD code = 0;
            if (GetExitCodeProcess(tunnelProc_.hProcess, &code) && code != STILL_ACTIVE)
                break;
            SLEEP_MS(10);
        }

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
