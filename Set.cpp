#include "Set.h"
#include "Shared.h"
#include "User.h"
#include "Tool.h"
#include "Message.h"

NOSTD; NOXS;

//是否应该进入设置界面
bool NeedEnterSetting = false;

//抗锯齿可选等级
static const int AAOptions[6] = { 0, 2, 4, 8, 16, 32 };
static const int AAOptionCount = 6;

//绘制玻璃条
static void DrawGlassBar(int x, int y, int w, int h, int r, RenderTarget& dest)
{
	static Color FillColor = Color(25, 25, 25, 170);
	static Color BorderColor = User::MainColor;
	static int BorderWidth = WindowSize.x / 1200;

	if (UpdateUser > 0)
	{
		BorderColor = User::MainColor;
	}

	XGraph::SetFillColor(FillColor);
	XGraph::SetColor(BorderColor);
	XGraph::LineShape::SetLineWidth(BorderWidth);

	XGraph::RectangleShape::FillRoundRect(x, y, w, h, r, dest);
}

//圆形滑块（带滑块缩放动画）
//返回当前值
static float DrawCircleSlider(
	int x, int y, int w, int h,
	float value, float minValue, float maxValue,
	EV& knobScale, RenWin& window)
{
	//轨道
	int trackH = max(2, h / 10);
	int trackY = y + h / 2 - trackH / 2;
	int trackX = x + h / 2;
	int trackW = w - h;

	XGraph::SetFillColor(Color(70, 70, 70));
	XGraph::RectangleShape::FillRoundRect_WithoutBorder(trackX, trackY, trackW, trackH, trackH / 2, window);

	//已填充部分
	float t = (value - minValue) / (maxValue - minValue);
	if (t < 0) t = 0;
	if (t > 1) t = 1;

	int fillW = (int)(trackW * t);
	if (fillW > 0)
	{
		XGraph::SetFillColor(User::MainColor);
		XGraph::RectangleShape::FillRoundRect_WithoutBorder(trackX, trackY, fillW, trackH, trackH / 2, window);
	}

	//滑块圆
	int knobR = (int)(h / 5 * knobScale.value / 100.0);
	int knobX = trackX + fillW;
	int knobY = y + h / 2;

	XGraph::SetFillColor(Color(255, 255, 255));
	XGraph::CircleShape::FillCircle_WithoutBorder(knobX, knobY, knobR, window);

	XGraph::SetColor(User::MainColor);
	XGraph::LineShape::SetLineWidth(max(1, WindowSize.x / 700));
	XGraph::CircleShape::Circle(knobX, knobY, knobR, window);

	//交互
	bool hover = XMsg::MouseMsg::IsMouseIn(x, y, w, h);
	if (hover) knobScale.SetAnimation(130, 12);
	else knobScale.SetAnimation(100, 12);

	if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft) || XMsg::KeyMsg::Keystate(VK::MouseLeft))
	{
		if (hover)
		{
			int mx = XMsg::MouseMsg::GetMousePosWindow().x;
			float nt = (mx - trackX) / (float)trackW;
			if (nt < 0) nt = 0;
			if (nt > 1) nt = 1;
			return minValue + (maxValue - minValue) * nt;
		}
	}

	return value;
}

//颜色插值
static Color LerpColor(const Color& StartColor, const Color& EndColor, float t)
{
	if (t < 0.f) t = 0.f;
	if (t > 1.f) t = 1.f;

	return Color(
		static_cast<uint8_t>(StartColor.r + (EndColor.r - StartColor.r) * t),
		static_cast<uint8_t>(StartColor.g + (EndColor.g - StartColor.g) * t),
		static_cast<uint8_t>(StartColor.b + (EndColor.b - StartColor.b) * t),
		static_cast<uint8_t>(StartColor.a + (EndColor.a - StartColor.a) * t)
	);
}

//开关按钮状态（每个开关独立保存动画状态）
struct ToggleState
{
	EV RoundPosX;
	Color RoundColor = Color(255, 255, 255);
	int BackAlpha = 0;
	bool inited = false;
};

//绘制开关按钮（参考 ToggleUI：滑块位移 + 颜色插值 + 背景淡入淡出）
static bool DrawSwitch(int x, int y, int w, int h, bool on, ToggleState& st, RenWin& window)
{
	const int TotalFrame = 12;
	const Color OffColor = Color(70, 70, 70);
	const Color OnColor = User::MainColor;
	const Color OnCircleColor = Color(255, 255, 255);

	int RoundSize = h / 2 - h / 8;
	int Off = RoundSize + w / 20;
	int On = w - RoundSize - w / 20;

	if (!st.inited)
	{
		st.inited = true;
		st.RoundPosX.SetAnimationStartValue(on ? On : Off);
		st.RoundPosX.SetAnimation(on ? On : Off, TotalFrame);
		st.RoundColor = on ? OnCircleColor : Color(255, 255, 255);
		st.BackAlpha = on ? 255 : 0;
	}

	if (st.RoundPosX.frame <= TotalFrame)
	{
		double t = st.RoundPosX.frame / (double)TotalFrame;

		if (on)
		{
			st.RoundPosX.UpdateAnimation(XEase::EaseBasic::easeOut, 4);
			st.RoundColor = LerpColor(OffColor, OnCircleColor, (float)t);
			st.BackAlpha = (int)(0 + (255 - 0) * XEase::EaseBasic::easeOut((float)t, 4));
		}
		else
		{
			st.RoundPosX.UpdateAnimation(XEase::EaseBasic::easeOut, 4);
			st.RoundColor = LerpColor(OnCircleColor, OffColor, (float)t);
			st.BackAlpha = (int)(255 + (0 - 255) * XEase::EaseBasic::easeOut((float)t, 4));
		}

		st.RoundPosX.frame++;
	}
	else
	{
		if (on)
		{
			st.RoundPosX.value = On;
			st.RoundColor = OnCircleColor;
			st.BackAlpha = 255;
		}
		else
		{
			st.RoundPosX.value = Off;
			st.RoundColor = OffColor;
			st.BackAlpha = 0;
		}
	}

	//背景
	XGraph::SetColor(OnColor);
	XGraph::RectangleShape::RoundRect(x, y, w, h, h / 2, window);
	XGraph::SetFillColor(XColor::AlphaColor(OnColor, st.BackAlpha));
	XGraph::RectangleShape::FillRoundRect_WithoutBorder(x, y, w, h, h / 2, window);

	//滑块
	XGraph::SetFillColor(st.RoundColor);
	XGraph::CircleShape::FillCircle_WithoutBorder(x + st.RoundPosX.value, y + h / 2, RoundSize, window);

	//交互
	if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
	{
		if (XMsg::MouseMsg::IsMouseIn(x, y, w, h))
		{
			if (on)
			{
				on = false;
				st.RoundPosX.SetAnimation(Off, TotalFrame);
			}
			else
			{
				on = true;
				st.RoundPosX.SetAnimation(On, TotalFrame);
			}
		}
	}
	return on;
}


void EnterSetting(RenWin& window)
{
	NeedEnterSetting = false;

	IMAGE temp;
	XImage::NewImage(window, temp);

	static int BarW = WindowSize.x / 20 * 8, BarH = WindowSize.x / 20 * 6;
	static int BarX = WindowSize.x / 2 - BarW / 2;

	EV BarY;
	BarY.SetAnimationStartValue(WindowSize.y);
	BarY.SetAnimation(WindowSize.y - BarH - WindowSize.x / 20 - WindowSize.x * 0.01 * 2, 30);
	EV BackAlpha;
	BackAlpha.SetAnimationStartValue(255);
	BackAlpha.SetAnimation(50, 30);

	bool NeedExit = false;

	//行高
	int RowH = BarH * 0.13;
	int RowX = BarX + BarW * 0.08;
	int RowW = BarW * 0.84;

	//每行入场动画：从下滑入 + 淡入
	const int RowCount = 4;
	EV RowOffset[RowCount];
	EV RowAlpha[RowCount];
	for (int i = 0; i < RowCount; i++)
	{
		RowOffset[i].SetAnimationStartValue(BarW * 0.15);
		RowOffset[i].SetAnimation(0, 30);
		RowAlpha[i].SetAnimationStartValue(0);
		RowAlpha[i].SetAnimation(255, 30);
	}

	//控件动画
	EV AAKnobScale;
	AAKnobScale.SetAnimationStartValue(100);
	ToggleState FixLineKnob, WriteAdjustKnob;


	//颜色块缩放动画
	EV ColorSwatchScale;
	ColorSwatchScale.SetAnimationStartValue(100);

	//实时修改：任何改动立即写入并保存
	auto Apply = [&]()
	{
		User::Save();
	};

	while (!XMsg::IsClose(window) && !NeedExit)
	{
		XWindow::DelayFps(window,60);

		BarY.UpdateAnimation(XEase::EaseBasic::easeOut, 6);
		BackAlpha.UpdateAnimation(XEase::EaseBasic::linear);

		//行入场动画更新（错峰）
		if(BarY.frame > 5)
		{
			for (int i = 0; i < RowCount; i++)
			{
				if (i == 0 || RowOffset[i - 1].frame > 3)
				{
					RowOffset[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
					RowAlpha[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
				}
			}
		}

		temp.color.a = BackAlpha.value;
		XImage::PutImage(temp, 0, 0, window);

		XGraph::SetFillColor(Color(50, 50, 50, 100));
		XGraph::RectangleShape::FillRoundRect_WithoutBorder(BarX, BarY.value, BarW, BarH, WindowSize.x / 100, window);
		DrawGlassBar(BarX, BarY.value, BarW, BarH, WindowSize.x / 100, window);

		//标题
		XText::SetFontConfig(Color::White, FONTSIZE * 1.2);
		XText::SetFontAdjust(ADJUST_CENTER, ADJUST_TOP);
		XText::Xyprintf(BarX + BarW / 2, BarY.value + BarH * 0.04, L"设置", window);

		int y = BarY.value + BarH * 0.16;

		//1. 主色
		{
			int ox = (int)RowOffset[0].value;
			int alpha = (int)RowAlpha[0].value;

			XText::SetFontConfig(Color(255, 255, 255, alpha), FONTSIZE * 1.2);
			XText::SetFontAdjust(ADJUST_LEFT, ADJUST_CENTER);
			XText::Xyprintf(RowX, y + RowH / 2 + ox, L"主题颜色", window);

			//颜色块（小一点）
			int sw = RowH * 0.5;
			int sx = RowX + RowW * 0.72;
			int sy = y + RowH / 2 - sw / 2 + ox;

			bool hover = XMsg::MouseMsg::IsMouseIn(sx, sy, sw, sw);
			ColorSwatchScale.SetAnimation(hover ? 115 : 100, 12);
			ColorSwatchScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);

			int s = (int)(sw * ColorSwatchScale.value / 100.0);
			int cx = sx + sw / 2, cy = sy + sw / 2;

			XGraph::SetFillColor(User::MainColor);
			XGraph::RectangleShape::FillRoundRect_WithoutBorder(cx - s / 2, cy - s / 2, s, s, WindowSize.x / 250, window);

			if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
			{
				if (hover)
				{
					User::MainColor = ChooseColorWindow(User::MainColor, window);
					XWindow::SetBackGroundColor(Color(30, 30, 30));
					Apply();
				}
			}
		}
		y += RowH;

		//3. 图形矫正
		{
			int ox = (int)RowOffset[2].value;
			int alpha = (int)RowAlpha[2].value;

			XText::SetFontConfig(Color(255, 255, 255, alpha), FONTSIZE * 1.2);
			XText::SetFontAdjust(ADJUST_LEFT, ADJUST_CENTER);
			XText::Xyprintf(RowX, y + RowH / 2 + ox, L"图形矫正", window);

			int bw = RowH * 0.8, bh = RowH * 0.4;
			int bx = RowX + RowW * 0.72;
			bool on = DrawSwitch(bx, y + RowH / 2 - bh / 2 + ox, bw, bh, User::EnableFixShape, FixLineKnob, window);
			if (on != User::EnableFixShape)
			{
				User::EnableFixShape = on;
				Apply();
			}
		}
		y += RowH;

		//4. 书写对齐线
		{
			int ox = (int)RowOffset[3].value;
			int alpha = (int)RowAlpha[3].value;

			XText::SetFontConfig(Color(255, 255, 255, alpha), FONTSIZE * 1.2);
			XText::SetFontAdjust(ADJUST_LEFT, ADJUST_CENTER);
			XText::Xyprintf(RowX, y + RowH / 2 + ox, L"书写对齐线", window);

			int bw = RowH * 0.8, bh = RowH * 0.4;
			int bx = RowX + RowW * 0.72;
			bool on = DrawSwitch(bx, y + RowH / 2 - bh / 2 + ox, bw, bh, User::EnableWriteAdjust, WriteAdjustKnob, window);
			if (on != User::EnableWriteAdjust)
			{
				User::EnableWriteAdjust = on;
				Apply();
			}
		}

		//回收
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			if (!XMsg::MouseMsg::IsMouseIn(BarX, BarY.value, BarW, BarH))
			{
				break;
			}
		}
	}


	BarY.SetAnimation(WindowSize.y, 30);
	BackAlpha.SetAnimation(255, 30);

	while (!XMsg::IsClose(window))
	{
		XWindow::DelayFps(window,60);

		BarY.UpdateAnimation(XEase::EaseBasic::easeOut, 6);
		BackAlpha.UpdateAnimation(XEase::EaseBasic::linear);

		temp.color.a = BackAlpha.value;
		XImage::PutImage(temp, 0, 0, window);

		if (BarY.value < WindowSize.y - WindowSize.x / 20)
		{
			XGraph::SetFillColor(Color(50, 50, 50, 100));
			XGraph::RectangleShape::FillRoundRect_WithoutBorder(BarX, BarY.value, BarW, BarH, WindowSize.x / 100, window);
			DrawGlassBar(BarX, BarY.value, BarW, BarH, WindowSize.x / 100, window);
		}

		if (!BackAlpha.IsAnimation()) break;
	}

	XMsg::ClearMsg();
	XMsg::SetSleepTime(10);
}
