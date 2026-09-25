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

NOXS; NOSTD;

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
	L"Ciallo-(< ' ω< )-/",
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

void SetWindowClassIconFromExe(sf::Window& window, int resId = 1)
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

	RenWin MainWindow;

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

	//设置字体
	XText::SetFont("FontLoader.dll");
	
	//初始化
	Init(MainWindow);
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