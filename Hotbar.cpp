#include "Hotbar.h"
#include "Shared.h"
#include "User.h"
#include "Tool.h"
#include "Write.h"
#include "BottomMessage.h"
#include "RightMessage.h"

NOXS; NOSTD;

void DrawX(RenderTarget& target, Vector2i Pos, int Size)
{
	XGraph::LineShape::Line(Pos.x - Size, Pos.y - Size, Pos.x + Size, Pos.y + Size, target);
	XGraph::LineShape::Line(Pos.x + Size, Pos.y - Size, Pos.x - Size, Pos.y + Size, target);
}

namespace
{
	inline wstring GetDirOfFile(const wstring& filePath)
	{
		std::error_code ec;
		filesystem::path p = filesystem::absolute(filesystem::path(filePath), ec);

		// parent_path 不含末尾分隔符，自己补上
		std::wstring dir = p.parent_path().wstring();
		if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/')
			dir += L'\\';

		return dir;
	}
}

class QuickChooseUI
{
	//内部圆环动画
	EV InRoundAngle;
	EV InRoundSize;

	//外环动画
	EV OutRoundAngle[6];
	EV OutRoundAlpha[6];
	EV OutRoundSize[6];

	int OutRoundBasicSize = WindowSize.x / 15;
	int InRoundBasicSize = WindowSize.x / 100;

	//图标
	IMAGE Icon[6];
	wstring IconName[6] = { L"橡皮擦",L"移动缩放",L"计时器",L"展台",L"草稿",L"笔" };
	
	Vector2i pos;

	//绘制内部动画
	void DrawInRound(RenWin& window)
	{
		//更新启动动画
		InRoundAngle.UpdateAnimation(XEase::EaseBasic::easeOut, 6);
		InRoundSize.UpdateAnimation(XEase::EaseBasic::easeOut, 6);

		//定义线宽
		static int lw = WindowSize.x / 500;
		XGraph::LineShape::SetLineWidth(lw);

		Color RounColor = XColor::AlphaColor(User::MainColor, 255 * InRoundAngle.frame / InRoundAngle.totalframe);
		if (!InRoundAngle.IsAnimation()) RounColor.a = 255;
		XGraph::SetColor(RounColor);
		
		XGraph::CircleShape::Arc(pos.x, pos.y, InRoundBasicSize * InRoundSize.value / 100, InRoundAngle.value / 5 - 180, InRoundAngle.value, window);
		DrawX(window, pos, InRoundBasicSize * InRoundSize.value / 200);
	}

	static constexpr int   kSlots = 6;
	static constexpr float kSlotArc = 360.f / kSlots;
	int CheckRadialMenu(
		const Vector2i& MousePos,
		const Vector2i& CenterPos,
		int OuterRadius,
		int InnerRadius,
		float RotateDeg = 0.f)
	{
		const float dx = (float)(MousePos.x - CenterPos.x);
		const float dy = (float)(MousePos.y - CenterPos.y);

		const float r2 = dx * dx + dy * dy;
		const float rO = (float)OuterRadius, rI = (float)InnerRadius;

		if (r2 > rO * rO) return -1;     // 外圈外
		if (r2 < rI * rI) return -2;     // 内圈内 → 点的是中间那个 ✕（取消）

		// y 向下 → atan2(dy,dx) 天然"顺时针为正"，不要翻 y
		float ang = std::atan2(dy, dx) * 180.f / 3.141592653589793f;

		ang -= RotateDeg;                // 菜单顺转 θ ⇔ 鼠标在菜单里逆转 θ

		// 把槽 0 的中心(-90°)旋到 0°，再补半格让分界落在槽间而非槽心
		ang = std::fmod(ang + 90.f + kSlotArc * 0.5f, 360.f);
		if (ang < 0.f) ang += 360.f;

		return (int)(ang / kSlotArc) % kSlots;   // 0~5
	}

	//绘制圆环动画
	bool DrawOutRound(RenWin& window)
	{
		//更新动画
		for (int i = 0; i < 6; i++)
		{
			if (i == 0)
			{
				OutRoundAngle[i].UpdateAnimation(XEase::EaseBasic::easeOut, 6);
				OutRoundAlpha[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
				OutRoundSize[i].UpdateAnimation(XEase::EaseBasic::easeOut, 6);
			}
			else if (OutRoundAngle[i - 1].frame > 1)
			{
				OutRoundAngle[i].UpdateAnimation(XEase::EaseBasic::easeOut, 6);
				OutRoundAlpha[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
				OutRoundSize[i].UpdateAnimation(XEase::EaseBasic::easeOut, 6);
			}
		}

		//绘制
		for (int i = 0; i < 6; i++)
		{
			auto& alpha = OutRoundAlpha[i].value;

			//检查透明度是否为0
			if (alpha <= 0) continue;

			static int lw = WindowSize.x / 500;
			XGraph::LineShape::SetLineWidth(lw);
			XGraph::SetColor(User::MainColor);

			Color BackColor = XColor::GrayColor(XColor::DarkColor(User::MainColor, 0.7), 0.7);
			XGraph::SetFillColor(XColor::AlphaColor(BackColor, OutRoundAlpha[i].value / 1.2));
			XGraph::CircleShape::FillArc(
				pos.x,
				pos.y,
				OutRoundBasicSize * OutRoundSize[i].value / 100, i * 60 + OutRoundAngle[i].value / 4 - 15, OutRoundAngle[i].value, window);

			double d = 2.0 * OutRoundBasicSize * OutRoundSize[i].value / 100 / M_PI;
			double theta = (i + 0.5) * M_PI / 3.0;

			static int fsp = WindowSize.x / 400;

			int ix = pos.x + d * cos(theta);
			int iy = pos.y + d * sin(theta);
			static float BasicIconScale = WindowSize.x / 40 / 512.0;
			float ImageScale = BasicIconScale * OutRoundSize[i].value / 100.0;
			XImage::PutScaleImage(Icon[i], ix, iy - fsp, ImageScale, ImageScale, window, 0.5, 0.5);

			static int fsize = WindowSize.x / 120;
			XText::SetFontConfig(Color(255, 255, 255, 255 * OutRoundSize[i].value / 100.0),fsize);
			XText::SetFontAdjust(ADJUST_CENTER, ADJUST_TOP);
			XText::Xyprintf(ix, iy + fsize + fsp, IconName[i], window);
		}

		//消息检测
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft) && XMsg::TouchMsg::GetTouchNum() < 2)
		{
			Vector2i MP = XMsg::MouseMsg::GetMousePosWindow();
			int ClickV = CheckRadialMenu(MP, pos, OutRoundBasicSize, InRoundBasicSize,60);
			if (ClickV > -1)
			{
				if (ClickV == 0) Tool::SetMainToolCount(0);
				if (ClickV == 1) Tool::SetMainToolCount(1);
				if (ClickV == 2) Tool::SetMainToolCount(3);
				if (ClickV == 3)
				{
					if (XFile::Exists(filesystem::current_path().wstring() + L"\\Extens\\Clocker\\MiuClock.exe"))
					{
						ShellExecuteW(
							NULL, L"open",
							(filesystem::current_path().wstring() + L"\\Extens\\Clocker\\MiuClock.exe").c_str(),
							NULL, GetDirOfFile(filesystem::current_path().wstring() + L"\\Extens\\Clocker\\MiuClock.exe").c_str(), SW_SHOW);
					}
					else
					{
						RightMessage::ShowMessage(
							L"无法加载计时器插件，错误代码：NOAPPLICATION", L"加载插件", RightMessageType_ERROR);

						BottomMessage::AddMessage(106, L"加载插件：计时器 失败", BottomMessageType_ERROR, 180);
					}
				}
				if (ClickV == 4)
				{
					Tool::StartCamera(window);
				}
				if (ClickV == 5)
				{
					Tool::StartTempLayer();
				}
			}

			return true;
		}

		return false;
	}

public:
	//初始化UI
	void init(RenWin& window, Vector2i UIPos)
	{
		static const int AnimationTotalFrame = 30;

		InRoundAngle.SetAnimation(320, AnimationTotalFrame);
		InRoundSize.SetAnimation(100, AnimationTotalFrame);

		for (int i = 0; i < 6; i++)
		{
			OutRoundAngle[i].SetAnimationStartValue(0);
			OutRoundAngle[i].SetAnimation(60, AnimationTotalFrame);
			OutRoundSize[i].SetAnimation(100, AnimationTotalFrame);
			OutRoundAlpha[i].SetAnimation(255, AnimationTotalFrame);
		}

		//加载图标
		static const wstring ImageName[6] = { L"Erase.dll",L"Move.dll",L"Clock.dll",L"Camera.dll",L"TempLayer.dll",L"Pen.dll" };
		for (int i = 0; i < 6; i++)
		{
			XImage::NewImage(Icon[i], ImgPath + L"\\QuickBar\\" + ImageName[i]);
		}

		pos = UIPos;
	}

	//绘制UI
	bool Main(RenWin& window)
	{
		//绘制外部圆环
		bool v = DrawOutRound(window);
		//绘制内部圆环
		DrawInRound(window);

		return v;
	}
};

void EnterQuickChoose(RenWin& window)
{
	//获取鼠标坐标作为工具显示坐标
	Vector2i MousePos = XMsg::MouseMsg::GetMousePosWindow();

	//获取窗口图像用来显示背景
	IMAGE BackImage;
	XImage::NewImage(window, BackImage);
	XWindow::SetBackGroundColor(User::BackColor);

	QuickChooseUI ui;
	ui.init(window, MousePos);

	while (XMsg::IsOpen(window))
	{
		XWindow::DelayFps(window, 60);

		XImage::PutImage(BackImage, 0, 0, window);

		//绘制
		if (ui.Main(window)) break;

	}

	XWindow::SetBackGroundColor(User::BackColor);
	XMsg::ClearMsg();
	XMsg::SetSleepTime(10);
}

void Hotbar::Main(RenWin& window)
{

	NOXS; NOSTD;

	//检查三指情况
	static int Clock3f = 0;

	int TouchNum = XMsg::TouchMsg::GetTouchNum();

	if (TouchNum > 2)
	{
		Clock3f += 1;
	}
	else
	{
		//在松开时检查触控时长并判断
		if (Clock3f > 0 && Clock3f < 20)
		{
			EnterQuickChoose(window);
		}

		Clock3f = 0;
	}

}