#include "Xs/Xs.h"
#include <random>
#include "Shared.h"
#include "User.h"
#include "Tool.h"
#include "Write.h"
#include "BottomMessage.h"
#include "RightMessage.h"
#include "SoundPlayer.h"
#include "Debug.h"
#include "Update.h"
#include "Error.h"
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

NOXS; NOSTD;

RenWin MainWindow;

//异常
#pragma region MyRegion

wstring ExceptionCodeToString(DWORD code)
{
	switch (code)
	{
	case EXCEPTION_ACCESS_VIOLATION:         return L"EXCEPTION_ACCESS_VIOLATION";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return L"EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_BREAKPOINT:               return L"EXCEPTION_BREAKPOINT";
	case EXCEPTION_DATATYPE_MISALIGNMENT:    return L"EXCEPTION_DATATYPE_MISALIGNMENT";
	case EXCEPTION_FLT_DENORMAL_OPERAND:     return L"EXCEPTION_FLT_DENORMAL_OPERAND";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return L"EXCEPTION_FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_INEXACT_RESULT:       return L"EXCEPTION_FLT_INEXACT_RESULT";
	case EXCEPTION_FLT_INVALID_OPERATION:    return L"EXCEPTION_FLT_INVALID_OPERATION";
	case EXCEPTION_FLT_OVERFLOW:             return L"EXCEPTION_FLT_OVERFLOW";
	case EXCEPTION_FLT_STACK_CHECK:          return L"EXCEPTION_FLT_STACK_CHECK";
	case EXCEPTION_FLT_UNDERFLOW:            return L"EXCEPTION_FLT_UNDERFLOW";
	case EXCEPTION_ILLEGAL_INSTRUCTION:      return L"EXCEPTION_ILLEGAL_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR:             return L"EXCEPTION_IN_PAGE_ERROR";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:       return L"EXCEPTION_INT_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW:             return L"EXCEPTION_INT_OVERFLOW";
	case EXCEPTION_INVALID_DISPOSITION:      return L"EXCEPTION_INVALID_DISPOSITION";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return L"EXCEPTION_NONCONTINUABLE_EXCEPTION";
	case EXCEPTION_PRIV_INSTRUCTION:         return L"EXCEPTION_PRIV_INSTRUCTION";
	case EXCEPTION_SINGLE_STEP:              return L"EXCEPTION_SINGLE_STEP";
	case EXCEPTION_STACK_OVERFLOW:           return L"EXCEPTION_STACK_OVERFLOW";
	default:
	{
		wchar_t buf[64];
		swprintf(buf, 64, L"UNKNOWN_EXCEPTION (0x%08X)", code);
		return std::wstring(buf);
	}
	}
}

// ---------- 获取时间戳 wstring ----------
wstring GetTimeStampW()
{
	time_t now = time(NULL);
	struct tm t;
	localtime_s(&t, &now);

	wchar_t buf[64];
	wcsftime(buf, 64, L"%Y%m%d %H%M%S", &t);
	return std::wstring(buf);
}

LONG WINAPI GlobalExceptionHandler(EXCEPTION_POINTERS* ep)
{
	DWORD code = ep->ExceptionRecord->ExceptionCode;
	PVOID addr = ep->ExceptionRecord->ExceptionAddress;

	XFile::CreateDirectory(L"DMP");

	// ---------- 1. 写 minidump ----------
	std::wstring dumpPath = std::wstring(filesystem::current_path().wstring()) + L"\\DMP\\"+ GetTimeStampW() + L"crash.dmp";
	HANDLE hFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION mei;
		mei.ThreadId = GetCurrentThreadId();
		mei.ExceptionPointers = ep;
		mei.ClientPointers = FALSE;

		MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			hFile,
			MiniDumpNormal,
			&mei,
			NULL,
			NULL
		);
		CloseHandle(hFile);
	}

	// ---------- 2. 写 wstring 日志 ----------
	std::wstring logPath = std::wstring(filesystem::current_path().wstring()) + L"\\DMP\\" + GetTimeStampW() + L"crash.log";
	FILE* f = _wfopen(logPath.c_str(), L"a");
	if (f)
	{
		std::wstring codeStr = ExceptionCodeToString(code);
		std::wstring timeStr = GetTimeStampW();

		fwprintf(f, L"[%s] Exception: %s\n", timeStr.c_str(), codeStr.c_str());
		fwprintf(f, L"  Code:       0x%08X\n", code);
		fwprintf(f, L"  Address:    0x%p\n", addr);

		// 如果是访问违例，额外记录访问类型和地址
		if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
		{
			DWORD accessType = ep->ExceptionRecord->ExceptionInformation[0];
			PVOID accessAddr = (PVOID)ep->ExceptionRecord->ExceptionInformation[1];
			fwprintf(f, L"  Access Type: %s\n", accessType == 0 ? L"Read" : (accessType == 1 ? L"Write" : L"Execute"));
			fwprintf(f, L"  Access Addr: 0x%p\n", accessAddr);
		}

		fwprintf(f, L"  Thread ID:  %u\n", GetCurrentThreadId());
		fwprintf(f, L"  Process ID: %u\n", GetCurrentProcessId());
		fwprintf(f, L"\n");
		fclose(f);
	}


	vector<wstring> Discription = {
		L"MiuBarrd发生严重错误并崩溃。",
		L"异常类型: " + ExceptionCodeToString(code),
		L"异常地址: 0x" + std::to_wstring((uintptr_t)addr),
		L"时间: " + GetTimeStampW(),
		L"dump 文件: DMP/" + GetTimeStampW() + L".dmp",
		L"重启后可能解决此问题。"
	};

	Error::ShowError(MainWindow, Discription, ExceptionCodeToString(code));

	return EXCEPTION_EXECUTE_HANDLER;
}

//测试
#pragma region MyRegion

void Test_CppException()
{
	throw std::runtime_error("测试 C++ 异常");
}

class Base
{
public:
	virtual void foo() = 0;
};

void Test_PureCall()
{
	Base* b = (Base*)malloc(sizeof(Base));  // 没构造，vtable 是 0
	b->foo();  // EXCEPTION_ILLEGAL_INSTRUCTION 或 0xC0000005
	free(b);
}

#pragma endregion


#pragma endregion

static int Random(int min, int max) {
	static thread_local std::mt19937 gen(std::random_device{}());
	std::uniform_int_distribution<int> dist(min, max);
	return dist(gen);
}

//初始化程序
void Init(RenWin& window)
{
	FONTSIZE = ScreenSize.x / 95;

	//禁用IME
	HIMC hImc = ImmGetContext(window.getNativeHandle());
	if (hImc) {
		ImmReleaseContext(window.getNativeHandle(), hImc);
	}
	ImmAssociateContext(window.getNativeHandle(), nullptr);

	static wstring MusicPath = filesystem::current_path().wstring() + L"\\Media\\";
	// 初始化音频
	player.Load(L"info", MusicPath + L"Info.wav");
	player.Load(L"warning", MusicPath + L"Warning.wav");
	player.Load(L"error", MusicPath + L"Error.wav");
	player.Load(L"success", MusicPath + L"Success.wav");

	//初始化调试模式
	ifstream DebugCheck("DEBUG");
	if (DebugCheck.is_open())
	{
		DebugCheck.close();
		Debug = true;
		User::MainColor = Color(255, 255, 100);
	}
	else Debug = false;
}

//启动动画
void StartAnimation(int PosStartX,int PosStartY,int SizeStartX,int SizeStartY, RenWin& window)
{
	XWindow::DWM::SetWindowDarkMode(window, true);
	XWindow::DWM::SetWindowBorderColor(window, User::MainColor);
	XWindow::DWM::SetWindowRoundCorner(window, CORNER_ROUND);
	XWindow::DWM::ExtendIntoClientArea(window, -1, -1, -1, -1);
	XWindow::DWM::SetWindowBackType(window,BACKTYPE_BLUR);

	wstring emoji[8] = {
	L"(p > w < q)",
	L"\\(*‘。’*)/",
	L"( ^ _ ^ )/",
	L"\\(‘ . ’ )\\",
	L"(‘ w ’)",
	L"^ _ ^",
	L"Ciallo-(< ' w < )-/",
	L"(-'。')-"
	};

	int emo = Random(0, 7);

	int clock_start = 0;
	int start_frame = 0;

	int WindowW = ScreenSize.x / 3;
	int WindowH = ScreenSize.y / 3;

	XText::SetFontColor(User::MainColor);
	XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);

	while (XMsg::IsOpen(window))
	{
		clock_start += 1;
		XWindow::DelayFps(window,60);
		WindowSize = XWindow::GetWindowSize(window);
		XGraph::SetFillColor(Color(30,30,30,150));
		XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, WindowSize.x, WindowSize.y, window);

		if (clock_start > 60)
		{
			if (start_frame <= 20)
			{
				double t = start_frame / (double)20;

				int w = ScreenSize.x / 3 + (ScreenSize.x - ScreenSize.x / 3) * XEase::EaseBasic::easeInBack(t, 2);
				int h = ScreenSize.y / 3 + (ScreenSize.y - 1 - ScreenSize.y / 3) * XEase::EaseBasic::easeInBack(t, 2);

				int xs = ScreenSize.x / 3, ys = ScreenSize.y / 3;
				int x = xs + (0 - xs) * XEase::EaseBasic::easeInBack(t, 2);
				int y = ys + (0 - ys) * XEase::EaseBasic::easeInBack(t, 2);

				XWindow::MoveWindow(window, x, y);
				XWindow::SetWindowSize(window, w, h);

				start_frame += 1;

				window.clear(Color::Transparent);
			}
			else
			{
				break;
			}
		}
		else
		{
			XText::SetFontSize(ScreenSize.x / 50);
			XText::Xyprintf(WindowSize.x / 2, WindowSize.y / 3, L"MiuBarrd白板", window);
			XText::SetFontSize(ScreenSize.x / 80);
			XText::Xyprintf(WindowSize.x / 2, WindowSize.y / 2, emoji[emo], window);
			XText::Xyprintf(WindowSize.x / 2, WindowSize.y / 1.6, L"---正在启动---", window);
		}
	}
}

void ShowTime(RenWin& window)
{
	static int clock = 0;

	XText::SetFontAdjust(ADJUST_RIGHT, ADJUST_TOP);
	static wstring text = to_string(XTime::GetTimeNow_Hour()) + L" : " + to_string(XTime::GetTimeNow_Min());
	if (clock < 300) clock += 1;
	else
	{
		int h = XTime::GetTimeNow_Hour(), m = XTime::GetTimeNow_Min();
		wstring Hs = to_wstring(h);
		wstring Ms;
		if (m > 9) Ms = to_wstring(m);
		else Ms = L"0" + to_wstring(m);

		text = Hs + L" : " + Ms;
		clock = 0;
	}

	static int FS = FONTSIZE * 2;
	XText::SetFontConfig(User::MainColor, FS);
	
	static int x = WindowSize.x * 0.99, y = WindowSize.y * 0.02;
	XText::Xyprintf(x, y, text,window);
}

//主逻辑
void App(RenWin& window)
{
	SetUnhandledExceptionFilter(GlobalExceptionHandler);

	XWindow::MoveWindow(window, 0, 1);
	XWindow::SetWindowSize(window, ScreenSize.x, ScreenSize.y - 1);

	XWindow::DWM::SetWindowRoundCorner(window, CORNER_NOROUND);
	XWindow::DWM::SetWindowBackType(window, BACKTYPE_NULL);
	XWindow::RemoveWindowExStyle(window,WS_EX_LAYERED);

	SetWindowPos(window.getNativeHandle(), HWND_DESKTOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	XWindow::SetBackGroundColor(Color(30, 30, 30));

	WindowSize = { ScreenSize.x, ScreenSize.y - 1 };

	Update::CheckUpdate(window);

	window.clear(Color(30, 30, 30));

	while (XMsg::IsOpen(window))
	{
		XWindow::DelayFps(window,60);

		if(window.hasFocus()) XSystem::Taskbar::SetTaskBarVisible(false);
		else XSystem::Taskbar::SetTaskBarVisible(true);

		//绘制书写
		Write::Show(window);

		//绘制工具
		Tool::Draw(window);

		//绘制消息框架
		BottomMessage::MessageManage(window);
		RightMessage::Manage(window);

		//书写逻辑
		Write::WriteT(window);
		
		while (!XWindow::IsFocus(window))
		{
			XMsg::UpdateMsg(window);
			::Sleep(10);
			XSystem::Taskbar::SetTaskBarVisible(true);
		}

		if (UpdateUser > 0) UpdateUser -= 1;
	}

	XSystem::Taskbar::SetTaskBarVisible(true);
}

static void SetWindowClassIconFromExe(sf::Window& window, int resId = 1)
{
	HWND hwnd = static_cast<HWND>(window.getNativeHandle());
	HINSTANCE hInst = GetModuleHandle(nullptr);

	// 大图标（Alt+Tab / 任务栏大视图）
	HICON hBig = (HICON)LoadImage(
		hInst,
		MAKEINTRESOURCE(resId),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXICON),
		GetSystemMetrics(SM_CYICON),
		LR_DEFAULTCOLOR);
	// 小图标（标题栏 / 任务管理器进程页）
	HICON hSmall = (HICON)LoadImage(
		hInst,
		MAKEINTRESOURCE(resId),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR);

	if (hBig)
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
	if (hSmall)
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);

	// 可选：补窗口类图标，防 DefWindowProc 回退
	if (hBig || hSmall) {
		SetClassLongPtr(hwnd, GCLP_HICON, (LONG_PTR)hBig);
		SetClassLongPtr(hwnd, GCLP_HICONSM, (LONG_PTR)hSmall);
	}
}

static bool SetForeWindow(HWND hWnd)
{
	if (!hWnd) return false;

	// 1. 判断窗口是否存在
	if (!IsWindow(hWnd))
		return false;

	// 2. 如果已经是前台窗口，直接返回
	if (GetForegroundWindow() == hWnd)
		return true;

	// 3. 线程附加（必须，否则 SetForegroundWindow 经常失败）
	DWORD currentThreadId = GetCurrentThreadId();
	DWORD foregroundThreadId = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

	if (currentThreadId != foregroundThreadId)
		AttachThreadInput(currentThreadId, foregroundThreadId, TRUE);

	// 4. 如果最小化，先恢复
	if (IsIconic(hWnd))
		ShowWindow(hWnd, SW_RESTORE);

	// 5. 切换到前台
	SetForegroundWindow(hWnd);
	SetActiveWindow(hWnd);

	// 6. 解除线程附加
	if (currentThreadId != foregroundThreadId)
		AttachThreadInput(currentThreadId, foregroundThreadId, FALSE);

	return true;
}

int main()
{
	//打开检测
	HWND hwnd = FindWindowW(NULL, L"MiuBarrd");
	if (IsWindow(hwnd))
	{
		SetForeWindow(hwnd);
		exit(0);
	}

	//使用手动批处理避免意外
	XBatch::SetBatchType(BATCH_HANDLED);

	User::Read();

	XWindow::SetDPIAware();

	ScreenSize = XSystem::Info::GetScreenSize();
	ScreenScale = ScreenSize.y / 1080.0;

	//创建窗口
	int wx = ScreenSize.x / 2.5, wy = ScreenSize.y / 2.5;
	Vector2i WindowCreateSize = { wx,wy };
	Vector2i WindowCreatePos = { ScreenSize.x / 2 - WindowCreateSize.x / 2,ScreenSize.y / 2 - WindowCreateSize.y / 2 };
	XWindow::CreateGraphWindow(
		MainWindow,
		WindowCreatePos.x, WindowCreatePos.y, WindowCreateSize.x, WindowCreateSize.y, 
		"MiuBarrd", Style::None);

	//设置图标
	XWindow::SetIcon(MainWindow, ImgPath + L"Icon.dll");
	SetWindowClassIconFromExe(MainWindow);

	//初始化
	Init(MainWindow);

	//设置字体
	if (XFile::Exists(L"FontLoader.dll"))
	{
		XText::SetFont("FontLoader.dll");
	}
	else
	{
		XWindow::MoveWindow(MainWindow, 0, 1);
		XWindow::SetWindowSize(MainWindow, ScreenSize.x, ScreenSize.y - 1);

		XWindow::DWM::SetWindowRoundCorner(MainWindow, CORNER_NOROUND);
		XWindow::DWM::SetWindowBackType(MainWindow, BACKTYPE_NULL);
		XWindow::RemoveWindowExStyle(MainWindow, WS_EX_LAYERED);

		SetWindowPos(MainWindow.getNativeHandle(), HWND_DESKTOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

		XText::FindFont("微软雅黑");
		WindowSize = ScreenSize;
		Error::ShowError(
			MainWindow, { L"MiuBarrd发生严重错误并崩溃了",L"错误原因：缺失关键文件",L"重新安装可以解决此问题",L"错误代码：FONTDLL_LOST"}, L"FONTDLL_LOST");
	}

	SetForeWindow(MainWindow.getNativeHandle());

	//先禁用右侧消息，启用在Tool::Show
	RightMessage::SetVisible(false);

	//进入启动动画
	StartAnimation(
		WindowCreatePos.x, WindowCreatePos.y, 
		WindowCreateSize.x, WindowCreateSize.y, MainWindow);

	if(Debug)
	{
		RightMessage::ShowMessage(
			L"启用 开发者模式", L"终端", RightMessageType_WARNING,true);
	}

	//进入主逻辑
	App(MainWindow);

	XSystem::Taskbar::SetTaskBarVisible(true);

	return 0;
}