#include "Debug.h"
#include "Shared.h"
#include "Write.h"
#include <pdhmsg.h>
#include <pdh.h>
#include <d3dkmthk.h>
#include <cwchar>
#include <algorithm>
#include <dxgi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <deque>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "onecoreuap.lib")

#define D3D_OK ((LONG)0)

NOXS; NOSTD;

bool Debug = false;

class ProcUsage
{
public:
    // luidLow: 目标卡 LUID.LowPart（如你 CSV 里的 0x00012238 = 独显）。
    //          传 0 = 自动：PDH 取本进程最忙 3D 引擎；D3DKMT 枚举首卡
    explicit ProcUsage(unsigned int luidLow = 0) noexcept
        : m_kernel(0), m_user(0), m_qpc(0)
        , m_cpuReady(false), m_cpu(0.f), m_gpu(0.f)
        , m_luidLow(luidLow)
        , m_query(nullptr), m_counter(nullptr)
        , m_pdhOk(false), m_pdhTried(false), m_lastStatus(0)
        , m_state(0)
        , m_d3dTried(false), m_d3dOk(false), m_d3dOwned(false)
        , m_adapter{}, m_adapterLuid{}, m_d3dNode(0)
        , m_d3dLastRunning(0), m_d3dLastTick(0)
    {
        LARGE_INTEGER f{}; QueryPerformanceFrequency(&f);
        m_freq = f.QuadPart ? f.QuadPart : 1;
        m_interval = m_freq / 2;                       // 0.5 秒一次

        LARGE_INTEGER n{}; QueryPerformanceCounter(&n);
        m_lastSample = n.QuadPart;

        sampleCpu();
        ensurePdh();
        ensureD3D();
    }

    ~ProcUsage() noexcept
    {
        if (m_counter) { PdhRemoveCounter(m_counter); m_counter = nullptr; }
        if (m_query) { PdhCloseQuery(m_query);      m_query = nullptr; }
        m_pdhOk = false;
        if (m_d3dOwned && m_adapter)
        {
            D3DKMT_CLOSEADAPTER ca{};                  // 只关自己开的句柄
            ca.hAdapter = m_adapter;
            D3DKMTCloseAdapter(&ca);
        }
        m_d3dOk = false;
    }

    ProcUsage(const ProcUsage&) = delete;
    ProcUsage& operator=(const ProcUsage&) = delete;

    // 每帧调一次。返回本进程 CPU 占用 0~100（占总机算力，多核跑满=100）
    float getCpuPercent() noexcept
    {
        LARGE_INTEGER n{}; QueryPerformanceCounter(&n);
        if (n.QuadPart - m_lastSample >= m_interval)
        {
            sampleCpu();
            sampleGpu();
            m_lastSample = n.QuadPart;
        }
        return m_cpu;
    }

    // 本进程 GPU 占用 0~100（3D 引擎，与任务管理器进程列同口径）
    float getGpuPercent() const noexcept { return m_gpu; }

    // 2=PDH 出数  1=D3DKMT 兜底出数  0=等首个样本  -1=全不可用
    int           getGpuState()   const noexcept { return m_state; }
    unsigned long getLastStatus() const noexcept { return m_lastStatus; }
    const char* getGpuSource()  const noexcept
    {
        switch (m_state)
        {
        case  2: return "PDH";
        case  1: return "D3DKMT";
        case -1: return "UNAVAILABLE";
        default: return "WARMUP";
        }
    }

private:
    //====================== CPU ======================
    void sampleCpu() noexcept
    {
        FILETIME c{}, e{}, k{}, u{};
        if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) return;

        const unsigned long long kernel =
            ((unsigned long long)k.dwHighDateTime << 32) | k.dwLowDateTime;
        const unsigned long long user =
            ((unsigned long long)u.dwHighDateTime << 32) | u.dwLowDateTime;

        LARGE_INTEGER now{}; QueryPerformanceCounter(&now);

        if (m_cpuReady)
        {
            const double wall = double(now.QuadPart - m_qpc) / double(m_freq);
            const double cpu = double((kernel - m_kernel) + (user - m_user)) / 1e7;
            if (wall > 0.0)
            {
                const int cores = getCoreCount();
                m_cpu = std::clamp(float(cpu / wall * 100.0 / double(cores)), 0.f, 100.f);
            }
        }

        m_kernel = kernel; m_user = user; m_qpc = now.QuadPart;
        m_cpuReady = true;
    }

    static int getCoreCount() noexcept
    {
        DWORD_PTR proc = 0, sys = 0;
        if (GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys) && proc)
        {
            int n = 0;
            for (DWORD_PTR m = proc; m; m &= m - 1) ++n;
            if (n > 0) return n;
        }
        SYSTEM_INFO si{}; GetSystemInfo(&si);
        return si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
    }

    //====================== GPU 主路：PDH ======================
    void ensurePdh() noexcept
    {
        if (m_pdhTried) return;
        m_pdhTried = true;

        if (PdhOpenQueryW(nullptr, 0, &m_query) != ERROR_SUCCESS)
        {
            m_query = nullptr; return;
        }

        static const wchar_t kPath[] = L"\\GPU Engine(*)\\Utilization Percentage";

        // ★ 必须 W 版：PdhAddCounter 是 ANSI，传宽串会被当窄串截成前 4 字节
        if (PdhAddCounterW(m_query, kPath, 0, &m_counter) != ERROR_SUCCESS)
        {
            m_counter = nullptr; PdhCloseQuery(m_query); m_query = nullptr; return;
        }

        PdhCollectQueryData(m_query);                  // 首样本建基线，丢弃
        m_pdhOk = true;
    }

    void samplePdh() noexcept
    {
        if (!m_pdhOk || !m_query || !m_counter) return;
        if (PdhCollectQueryData(m_query) != ERROR_SUCCESS) return;

        DWORD bytes = 0, count = 0;
        const PDH_STATUS probe =
            PdhGetFormattedCounterArrayW(m_counter, PDH_FMT_DOUBLE, &bytes, &count, nullptr);
        if (probe != PDH_MORE_DATA || bytes == 0)
        {
            m_lastStatus = (unsigned long)probe; return;
        }

        // 结构含 LONGLONG(8字节对齐)；用 BYTE 缓冲避免 vector 构造把内存写花
        std::vector<BYTE> buf(bytes + 32, 0);
        auto* arr = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buf.data());
        if (PdhGetFormattedCounterArrayW(m_counter, PDH_FMT_DOUBLE, &bytes, &count, arr)
            != ERROR_SUCCESS)
            return;

        const DWORD pid = GetCurrentProcessId();
        wchar_t key[32];
        swprintf(key, 32, L"pid_%lu_", pid);           // 带尾下划线，防 pid_1 命中 pid_10

        wchar_t luidKey[48];
        luidKey[0] = 0;
        if (m_luidLow)
            swprintf(luidKey, 48, L"luid_0x%08X", m_luidLow);

        double renderMax = 0.0;                        // 3D / Graphics / Compute
        double otherSum = 0.0;                        // Copy / Video / Overlay
        bool   hit = false;

        for (DWORD i = 0; i < count; ++i)
        {
            if (!arr[i].szName) continue;

            // ★★ 核心：VALID_DATA=0 与 NEW_DATA=1 都有效，只有负数才是错误。
            //    写成 != ERROR_SUCCESS 会把"数值正在变化"的样本全丢 → 恒为 0
            if ((long)arr[i].FmtValue.CStatus < 0)
            {
                m_lastStatus = (unsigned long)arr[i].FmtValue.CStatus; continue;
            }

            const wchar_t* name = arr[i].szName;
            if (!wcsstr(name, key))                   continue;   // 只要本进程
            if (luidKey[0] && !wcsstr(name, luidKey)) continue;   // 只要指定卡

            const double v = std::clamp(arr[i].FmtValue.doubleValue, 0.0, 100.0);
            const Engine e = classify(name);
            hit = true;

            if (e == Engine::_3D || e == Engine::Graphics || e == Engine::Compute)
            {
                if (v > renderMax) renderMax = v;      // 多结点取最忙，不求和不平均
            }
            else if (e != Engine::Other)
            {
                otherSum += v;
            }
        }

        if (!hit) return;                              // 本进程还没在 GPU 上开队列

        double g = renderMax;
        if (g <= 0.0 && otherSum > 0.0) g = otherSum;  // 只在搬数据/解码时也算数
        if (g > 100.0) g = 100.0;

        m_gpu = (float)g;
        m_state = 2;
    }

    //====================== GPU 兜底：D3DKMT（进程级真值）======================
    void ensureD3D() noexcept
    {
        if (m_d3dTried) return;
        m_d3dTried = true;

        if (!m_luidLow)
        {
            // 没指定卡：枚举适配器，挑第一个（通常是主设备）
            D3DKMT_ENUMADAPTERS2 ea{};
            if (D3DKMTEnumAdapters2(&ea) != D3D_OK || !ea.NumAdapters) return;
            std::vector<D3DKMT_ADAPTERINFO> list(ea.NumAdapters);
            ea.pAdapters = list.data();
            ea.NumAdapters = (ULONG)list.size();
            if (D3DKMTEnumAdapters2(&ea) != D3D_OK || !ea.NumAdapters) return;
            m_adapterLuid = list[0].AdapterLuid;
        }
        else
        {
            m_adapterLuid.LowPart = m_luidLow;
            m_adapterLuid.HighPart = 0;                // 你 CSV 里 HighPart 确为 0
        }

        // D3DKMT_OPENADAPTERFROMLUID（注意是 LUID 不是 UID）
        D3DKMT_OPENADAPTERFROMLUID oa{};
        oa.AdapterLuid = m_adapterLuid;
        if (D3DKMTOpenAdapterFromLuid(&oa) != D3D_OK) return;

        m_adapter = oa.hAdapter;                    // 0 表示用已有设备句柄
        m_adapterLuid = oa.AdapterLuid;                // 回读真实 LUID
        m_d3dOwned = (m_adapter != 0);               // 只有自己开的才负责关
        m_d3dOk = true;

        primeD3DNode();
    }

    // 探测本进程在该卡实际用到哪个结点（0~3），打一格基线
    void primeD3DNode() noexcept
    {
        if (!m_d3dOk) return;
        LARGE_INTEGER n{}; QueryPerformanceCounter(&n);

        for (UINT node = 0; node < 4; ++node)
        {
            D3DKMT_QUERYSTATISTICS qs{};
            qs.Type = D3DKMT_QUERYSTATISTICS_PROCESS_NODE;
            qs.AdapterLuid = m_adapterLuid;

            // union 写法：QueryNode 与 QueryProcessNode 共用内存，
            // PID 所在分支是匿名 struct，没有 QueryNode.ProcessId 这个成员。
            // 先清 union 段，再按「PID + NodeId」顺序写，后写不覆盖先写。
            ::memset(&qs.QueryNode, 0, sizeof(qs.QueryNode));
            qs.QueryProcessNode.NodeId = GetCurrentProcessId();
            qs.QueryNode.NodeId = node;

            if (D3DKMTQueryStatistics(&qs) != 0) continue;   // 该结点不存在/无权限

            m_d3dNode = node;
            m_d3dLastRunning =
                qs.QueryResult.ProcessNodeInformation.RunningTime.QuadPart;
            m_d3dLastTick = n.QuadPart;
            return;
        }
        m_d3dOk = false;          // 本进程在这卡上一个结点都没用
    }

    void sampleD3D() noexcept
    {
        if (!m_d3dOk) { if (m_pdhTried) primeD3DNode(); return; }

        LARGE_INTEGER n{}; QueryPerformanceCounter(&n);
        if (n.QuadPart - m_d3dLastTick < m_interval) return;

        D3DKMT_QUERYSTATISTICS qs{};
        qs.Type = D3DKMT_QUERYSTATISTICS_PROCESS_NODE;
        qs.AdapterLuid = m_adapterLuid;

        ::memset(&qs.QueryNode, 0, sizeof(qs.QueryNode));
        qs.QueryProcessNode.NodeId = GetCurrentProcessId();
        qs.QueryNode.NodeId = m_d3dNode;

        if (D3DKMTQueryStatistics(&qs) != 0) { m_d3dOk = false; return; }

        if (D3DKMTQueryStatistics(&qs) != D3D_OK) { m_d3dOk = false; return; }

        // RunningTime 不是墙钟，是 100ns 单位的「结点忙碌时长」
        const LONGLONG cur = qs.QueryResult.ProcessNodeInformation.RunningTime.QuadPart;
        const LONGLONG dt = n.QuadPart - m_d3dLastTick;

        if (m_d3dLastRunning && cur >= m_d3dLastRunning && dt > 0)
        {
            const double busySec = double(cur - m_d3dLastRunning) * 100.0 / 1e9;
            const double wallSec = double(dt) / double(m_freq);
            if (wallSec > 0.0)
            {
                m_gpu = std::clamp(float(busySec / wallSec * 100.0), 0.f, 100.f);
                if (m_state != 2) m_state = 1;
            }
        }

        m_d3dLastRunning = cur;
        m_d3dLastTick = n.QuadPart;
    }

    //====================== 调度 ======================
    void sampleGpu() noexcept
    {
        if (!m_pdhTried) ensurePdh();                  // 失败可重试，不永久锁死
        if (m_pdhOk)     samplePdh();
        if (m_state != 2) sampleD3D();                 // PDH 没出数才走兜底
        if (m_state == 0 && !m_pdhOk && !m_d3dOk) m_state = -1;
    }

    //====================== 引擎分类 ======================
    // 顺序敏感：VideoDecode/Encode 必须先于 3D 判断
    enum class Engine { Other, Copy, Video, Overlay, Graphics, _3D, Compute };

    static Engine classify(const wchar_t* s) noexcept
    {
        if (!s) return Engine::Other;
        if (wcsstr(s, L"engtype_VideoDecode") ||
            wcsstr(s, L"engtype_VideoProcessing") ||
            wcsstr(s, L"engtype_VideoEncode"))        return Engine::Video;
        if (wcsstr(s, L"engtype_3D"))                return Engine::_3D;
        if (wcsstr(s, L"engtype_Graphics"))          return Engine::Graphics;
        if (wcsstr(s, L"engtype_Compute"))           return Engine::Compute;
        if (wcsstr(s, L"engtype_Overlay"))           return Engine::Overlay;
        if (wcsstr(s, L"engtype_LegacyOverlay"))     return Engine::Overlay;
        if (wcsstr(s, L"engtype_Copy"))              return Engine::Copy;
        return Engine::Other;
    }

    //====================== 成员（声明顺序 = 初始化顺序）======================
    unsigned long long m_kernel, m_user, m_qpc;
    long long   m_freq, m_interval, m_lastSample;
    bool        m_cpuReady;
    float       m_cpu, m_gpu;

    unsigned int m_luidLow;

    PDH_HQUERY   m_query;
    PDH_HCOUNTER m_counter;
    bool         m_pdhOk, m_pdhTried;
    unsigned long m_lastStatus;
    int          m_state;

    bool        m_d3dTried, m_d3dOk, m_d3dOwned;
    D3DKMT_HANDLE m_adapter;
    LUID        m_adapterLuid;
    UINT        m_d3dNode;
    LONGLONG    m_d3dLastRunning;
    LONGLONG    m_d3dLastTick;
};
ProcUsage usage;

int GetPs10(float v)
{
    int v10 = v / 10;

    int s = int(v) % 10;
    if (s >= 5) v10 += 1;

    return v10;
}

//帧率
inline float GetFps()
{
    static sf::Clock clock;
    static int frames = 0;
    static float fps = 0.f;

    ++frames;

    const float elapsed = clock.getElapsedTime().asSeconds();
    if (elapsed >= 1.0f)
    {
        fps = frames / elapsed;
        frames = 0;
        clock.restart();
    }

    return fps;
}

deque<wstring> msgs;

void DrawLeft(RenWin& window)
{
    static Vector2i Pos = { int(WindowSize.x * 0.01),int(WindowSize.x * 0.01) };
    XText::SetFontAdjust(ADJUST_LEFT, ADJUST_TOP);
    static int fsize = WindowSize.x / 80;
    XText::SetFontConfig(Color(255, 255, 100), fsize);
    XText::Xyprintf(Pos.x, Pos.y, L"调试模式 VERSION_" + XString::Convert::utf8_to_wstring(VER), window);
    static int Space = WindowSize.x * 0.015;
    XText::Xyprintf(Pos.x, Pos.y + Space, L"FPS >> " + to_wstring(GetFps()), window);

    float CPU = usage.getCpuPercent(), GPU = usage.getGpuPercent();

    wstring CpuText = L"CPU >> [";
    int countCpu = GetPs10(CPU);
    for (int i = 0; i < countCpu; i++) CpuText += L"l";
    for (int i = 0; i < 10 - countCpu; i++) CpuText += L" ";
    CpuText += L"] " + to_wstring(CPU) + L"%";

    wstring GpuText = L"CPU >> [";
    int countGpu = GetPs10(GPU);
    for (int i = 0; i < countGpu; i++) GpuText += L"l";
    for (int i = 0; i < 10 - countGpu; i++) GpuText += L" ";
    GpuText += L"] " + to_wstring(GPU) + L"%";

    XText::Xyprintf(Pos.x, Pos.y + Space * 2, CpuText, window);
    XText::Xyprintf(Pos.x, Pos.y + Space * 3, GpuText, window);

    Vector2i MousePos = XMsg::MouseMsg::GetMousePosWindow();
    XText::Xyprintf(Pos.x, Pos.y + Space * 4, L"TOUCHPOS >> " + to_wstring(MousePos.x) + L" " + to_wstring(MousePos.y),window);
    int TouchNum = XMsg::TouchMsg::GetTouchNum();
    XText::Xyprintf(Pos.x, Pos.y + Space * 5, L"TOUCHNUM >> " + to_wstring(TouchNum), window);

    int WriteSize = WriteUsage::GetSize();
    XText::Xyprintf(Pos.x, Pos.y + Space * 6, L"DataSize >> " + to_wstring(WriteSize), window);
}

void DrawRight(RenWin& window)
{
    static Vector2i Pos = { int(WindowSize.x * 0.99),int(WindowSize.x * 0.01) };
    XText::SetFontAdjust(ADJUST_RIGHT, ADJUST_TOP);
    static int fsize = WindowSize.x / 100;
    XText::SetFontConfig(Color(255, 255, 100), fsize);
    XText::Xyprintf(Pos.x, Pos.y, L"_DEBUGMESSAGE", window);
    static int Space = WindowSize.x * 0.01;

    for (int i = 0; i < msgs.size(); i++)
    {
        int x = Pos.x, y = Pos.y + Space * (i + 1);
        XText::Xyprintf(x, y,msgs[i], window);
    }
}

void DebugClass::Draw(RenWin& window)
{
    if (!Debug) return;

    DrawLeft(window);
    DrawRight(window);
}

void DebugClass::PushMsg(wstring msg)
{
    if (!Debug) return;

    msgs.push_back(msg);
    if (msgs.size() > 20) msgs.pop_front();
}