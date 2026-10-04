#include "Error.h"
#include "Shared.h"
#include "User.h"

NOXS; NOSTD;

void Error::ShowError(RenWin& window, std::vector<std::wstring> Discription, wstring ErrorCode)
{
	XWindow::SetBackGroundColor(Color(20, 0, 0));

	IMAGE img;
	if(!User::EnableAnimation) XImage::NewImage(img, ImgPath + L"Error\\Error.dll");
	else XImage::NewImage(img, ImgPath + L"Error\\ErrorAnimation.dll");

	int RestartClock = 300;

	XSystem::Taskbar::SetTaskBarVisible(true);

	static float Scale = WindowSize.x * 0.3 / 512.0;

	int FontSize = WindowSize.x / 50;
	int Space = WindowSize.y * 0.05;
	int x = WindowSize.x * 0.02;
	while (1)
	{
		XWindow::DelayFps(window);

		XText::SetFontConfig(Color::White, FontSize);
		XText::SetFontAdjust(ADJUST_LEFT, ADJUST_TOP);

		int y = WindowSize.y * 0.1;

		for (int i = 0; i < Discription.size(); i++)
		{
			XText::Xyprintf(x, y, Discription[i], window);
			y += Space;
		}

		y += Space;
		RestartClock -= 1;
		XText::Xyprintf(x, y, L"重新启动计时：" + to_wstring(RestartClock / 30 + 1), window);

		XImage::PutScaleImage(img, WindowSize.x * 0.8, WindowSize.y * 0.25, Scale, Scale, window, 1, 0.5);

		if (RestartClock < 0) break;
	}

	if(XFile::Exists(L"Fix.exe"))
	{
		ShellExecuteW(NULL, L"open", L"Fix.exe", NULL, NULL, NULL);
	}

	exit(-1);
}