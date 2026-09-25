#include "Tool.h"
#include "Shared.h"
#include "Debug.h"
#include "Message.h"
#include "User.h"
#include "BottomMessage.h"
#include "RightMessage.h"
#include "Write.h"
#include "Set.h"
#include "Camera.h"


/// <summary>
///草泥马币
/// </summary>

NOSTD; NOXS;

//辅助函数
#pragma region MyRegion

template<typename... Args>
static bool IsAnimations(Args&&... args) {
	return (... || args.IsAnimation());
}

static bool LoadIcon(IMAGE& img, wstring path)
{
	if (!XImage::NewImage(img, path))
	{
		//执行错误处理
		XImage::NewImage(img, ImgPath + L"Error.dll");

		return false;
	}
	return true;
}

static Vector2u ITU(Vector2i t)
{
	return { unsigned int(t.x),unsigned int(t.y) };
}

static Vector2u FTU(Vector2f t)
{
	return { unsigned int(t.x),unsigned int(t.y) };
}

static void DrawGlassBar(int x, int y, int w, int h, int r,RenderTarget& dest)
{
	//静态填充颜色
	static Color FillColor = Color(25, 25, 25, 170);
	//静态外框颜色
	static Color BorderColor = User::MainColor;
	//静态边框大小
	static int BorderWidth = WindowSize.x / 1200;

	if (UpdateUser > 0)
	{
		BorderColor = User::MainColor;
	}

	//设置颜色
	XGraph::SetFillColor(FillColor);
	XGraph::SetColor(BorderColor);
	XGraph::LineShape::SetLineWidth(BorderWidth);

	//绘制
	XGraph::RectangleShape::FillRoundRect(x, y, w, h, r, dest);
}

//提取后缀名
wstring GetFileExtension(const wstring& path)
{
	size_t dot = path.find_last_of(L'.');
	if (dot == wstring::npos || dot == path.size() - 1)
		return L"";
	return path.substr(dot + 1);
}

#pragma endregion

int Tool::ToolCount = 0;
int Tool::PenCap = 0;
Color Tool::PenColor = Color::White;
int Tool::PenSize;
bool Tool::IsInBar = false;

//预设笔粗细
static int PerSetPenSize[4];

//预设颜色表
const Color PersetPenColor[15] = {
	Color(255,255,255),
	Color(255,0,70),
	Color(255,253,85),
	Color(97, 255, 210),
	Color(150,160,250),
	Color(150,255,150),
	Color(190,130,255),
	Color(255,200,150),
	Color(255,153,204),
	Color(160,80,255),
	Color(150,200,255),
	Color(255,40,150),
	Color(255,160,70),
	Color(240,130,80),
	Color(240,90,230)
};

//UI
#pragma region MyRegion

//基准大小
static int BasicSize;
//图标大小
static int IconSize;
//UI空隙
static int UISpace;
//UI圆角大小
static int RoundSize;
//总动画帧数
static int TotalFrame;
//整体UI透明度
static int UIAlpha = 200;
//未激活工具透明度
static int NoActiveIconAlpha = 205;
//收缩时间 
static int EXP_TIME = 60;
//收缩缩放强度
static int ScaleStrgengh = 70;
static int MinFrame;

static void UpdateMoreBarVisible();
static void UpdatePageBar();

//页面选择器
bool NeedEnterChoosePage = false;
void EnterChoosePage(RenWin& window);

//导入管理
#pragma region MyRegion

//是否应该进入导入界面
static bool NeedEnterGet = false;

void Get(RenWin& window)
{
	NeedEnterGet = false;

	HWND FHWND = window.getNativeHandle();
	vector<Path> path =
		XFile::Pick::PickFiles(
			L"选择要导入的文件",
			{ {L"MiuBarrd支持的文件",L"*.png;*.jpg;*.jpeg;*.bmp;*.mwf"} },
			XFile::GetDir::Desktop().wstring(), false, FHWND);

	if (!path.empty())
	{
		wstring P = path[0].wstring();
		wstring ext = GetFileExtension(P);

		//图片文件
		if (ext == L"png" || ext == L"jpg" || ext == L"jepg" || ext == L"bmp")
		{
			if (!ImageManager::AddImage(P, window))
			{
				RightMessage::ShowMessage(L"导入图片失败，它好像并不是正常的图片", L"导入图片错误", RightMessageType_ERROR, true);
			}
		}
		//板书文件
		else
		{
			WriteFile::Load(P);
			UpdatePageBar();
		}
	}

	XMsg::ClearMsg();
	XMsg::SetSleepTime(30);
}

#pragma endregion

//保存
#pragma region MyRegion

//选择保存的格式

int ChooseType(RenWin& window)
{
	int choose = 0;

	RenWin temp;
	XWindow::CreateGraphWindow(temp, -1, -1, WindowSize.x / 2, WindowSize.y / 2, L"选择保存的格式", Style::Titlebar | Style::Close);
	XWindow::DWM::SetWindowTitleBarColor(temp, XWindow::GetBackGroundColor());
	XWindow::SetOwnerWindow(window);
	XWindow::DWM::SetWindowBorderColor(temp, User::MainColor);

	const int ChooseAll = 2;
	int BarW = WindowSize.x / 5, BarH = (WindowSize.y / 2 - (ChooseAll + 1) * UISpace) / ChooseAll;

	static IMAGE Icon[ChooseAll];
	static wstring Name[ChooseAll] = { L"Mwf板书文件",L"Png图片文件" };

	static bool init = false;
	if (!init)
	{
		static wstring ImgName[ChooseAll] = { L"MWF.dll",L"PNG.dll" };

		for (int i = 0; i < ChooseAll; i++)
		{
			LoadIcon(Icon[i], ImgPath + L"SaveType\\" + ImgName[i]);
		}

		init = true;
	}

	static int ButtomW = WindowSize.x / 20, ButtomH = WindowSize.y / 30;

	while (1)
	{
		XWindow::DelayFps(temp);

		for (int i = 0; i < ChooseAll; i++)
		{
			static int x = WindowSize.x / 100;
			int y = UISpace + (BarH + UISpace) * i;

			XGraph::SetFillColor(Color(20, 20, 20));
			XGraph::RectangleShape::FillRoundRect_WithoutBorder(x, y, BarW, BarH, RoundSize, temp);
			if (choose == i)
			{
				XGraph::LineShape::SetLineWidth(WindowSize.x / 1000);
				XGraph::SetColor(User::MainColor);
				XGraph::RectangleShape::RoundRect(x, y, BarW, BarH, RoundSize, temp);
			}

			static float Scale = IconSize * 2 / 512.0;

			XImage::PutScaleImage(Icon[i], x + BarW * 0.1, y + BarH / 2, Scale, Scale, temp,0,0.5);

			XText::SetFontConfig(Color::White, FONTSIZE);
			XText::SetFontAdjust(ADJUST_RIGHT, ADJUST_BOTTOM);
			XText::Xyprintf(x + BarW * 0.9, y + BarH * 0.9, Name[i], temp);

			if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
			{
				if (XMsg::MouseMsg::IsMouseIn(x, y, BarW, BarH))
				{
					choose = i;
				}
			}
		}

		XGraph::LineShape::SetLineWidth(WindowSize.x / 500);
		XGraph::SetColor(User::MainColor);
		XGraph::LineShape::Line(WindowSize.x / 100 + BarW * 1.2,BarH * 0.1, WindowSize.x / 100 + BarW * 1.2, WindowSize.y / 2 - BarH * 0.1, temp);

		//按钮显示
		for (int i = 0; i < 2; i++)
		{
			XGraph::SetFillColor(i == 0 ? User::MainColor : Color(120, 120, 120));
			XGraph::RectangleShape::FillRoundRect_WithoutBorder(
				WindowSize.x / 2 - (ButtomW + UISpace) * (i + 1), WindowSize.y / 2 - ButtomH - UISpace, 
				ButtomW, ButtomH, ButtomH / 3, temp);

			static wstring name[2] = { L"保存",L"取消" };
			XText::SetFontConfig(i == 0 ? Color(30, 30, 30) : Color::White, FONTSIZE);
			XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
			XText::Xyprintf(
				WindowSize.x / 2 - (ButtomW + UISpace) * (i + 1) + ButtomW / 2,
				WindowSize.y / 2 - ButtomH - UISpace + ButtomH / 2,
				name[i], temp);

			if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
			{
				if (XMsg::MouseMsg::IsMouseIn(
					WindowSize.x / 2 - (ButtomW + UISpace) * (i + 1), WindowSize.y / 2 - ButtomH - UISpace,
					ButtomW, ButtomH))
				{
					if (i == 1)
					{
						XMsg::ResetCloseMsg();
						return -1;
					}
					else
					{
						XMsg::ResetCloseMsg();
						return choose;
					}
				}
			}
		}

		//描述显示
		if (choose == 0)
		{
			static vector<wstring> dis= {
				L"MiuBarrd的板书文件",
				L"可以在MiuBarrd中打开",
				L"适合以后继续书写或更改"
			};

			XText::SetFontConfig(Color::White, FONTSIZE);
			XText::SetFontAdjust(ADJUST_LEFT, ADJUST_CENTER);
			for(int i = 0;i<dis.size();i++)
				XText::Xyprintf(WindowSize.x / 100 + BarW * 1.3, BarH * 0.5 + UISpace * i * 1.3, dis[i], temp);
		}
		if (choose == 1)
		{
			static vector<wstring> dis = {
				L"跨平台的图片文件",
				L"可以在任何设备中打开",
				L"适合长期保存和共享板书内容"
			};

			XText::SetFontConfig(Color::White, FONTSIZE);
			XText::SetFontAdjust(ADJUST_LEFT, ADJUST_CENTER);
			for (int i = 0; i < dis.size(); i++)
				XText::Xyprintf(WindowSize.x / 100 + BarW * 1.3, BarH * 0.5 + UISpace * i * 1.3, dis[i], temp);
		}

		if (XMsg::IsClose(temp))
		{
			XMsg::ResetCloseMsg();
			return -1;
		}
	}

	XMsg::ResetCloseMsg();

	return -1;
}

static bool NeedEnterSave = false;

void EnterSaveOrOpen(RenWin& window)
{
	NeedEnterSave = false;

	HWND FHWND = window.getNativeHandle();
	Path path =
		XFile::Pick::PickFolder(
			L"选择要保存的位置",
			XFile::GetDir::Desktop().wstring(), FHWND);

	wstring P = path.wstring();

	if (!P.empty())
	{

		int choose = ChooseType(window);

		if(choose == 0)
		{
			wstring P = XFile::GetDir::Desktop().wstring() + L"\\"
				+ to_wstring(XTime::GetTimeNow_Day()) + L"日" + to_wstring(XTime::GetTimeNow_Hour()) + L"时"
				+ to_wstring(XTime::GetTimeNow_Min()) + L"分" + L"板书";

			WriteFile::Save(P,L"mwf");
		}
		if (choose == 1)
		{
			wstring P = XFile::GetDir::Desktop().wstring() + L"\\"
				+ to_wstring(XTime::GetTimeNow_Day()) + L"日" + to_wstring(XTime::GetTimeNow_Hour()) + L"时"
				+ to_wstring(XTime::GetTimeNow_Min()) + L"分" + L"板书";

			WriteFile::Save(P,L"png");
		}
	}

	XMsg::ClearMsg();
	XMsg::SetSleepTime(30);
	XMsg::UpdateMsg(window);
}

#pragma endregion

//最小化
#pragma region MyRegion

//是否应该进入最小化
static bool NeedEnterMin = false;

//最小化
void MinExe(RenWin& window)
{
	NeedEnterMin = false;

	XWindow::SetBackGroundColor(Color::Transparent);

	IMAGE temp;
	XImage::NewImage(window, temp);

	//图片缩放
	EV ImgScale;
	ImgScale.SetAnimationStartValue(100);
	ImgScale.SetAnimation(65, TotalFrame);

	EV ImgY;
	ImgY.SetAnimationStartValue(WindowSize.y / 2);
	ImgY.SetAnimation(-WindowSize.y / 2);

	while (!XMsg::IsClose(window))
	{
		XWindow::DelayFps(window,60);

		XSystem::Taskbar::SetTaskBarVisible(true);
		ImgScale.UpdateAnimation(XEase::EaseBasic::easeOut, 4);
		if(!ImgScale.IsAnimation()) ImgY.UpdateAnimation(XEase::EaseBasic::easeIn, 4);
		if (!ImgY.IsAnimation()) break;


		float Scale = ImgScale.value / 100.0;
		XImage::PutScaleImage(temp, WindowSize.x / 2, ImgY.value, Scale, Scale, window,0.5,0.5);

		XGraph::SetColor(User::MainColor);
		static int lw = WindowSize.x / 250;
		XGraph::LineShape::SetLineWidth(lw);
		XGraph::RectangleShape::RoundRect(
			WindowSize.x / 2 * (1 - Scale), ImgY.value - WindowSize.y / 2 * Scale, WindowSize.x * Scale, WindowSize.y * Scale, RoundSize,window);
	}

	//最小化
	ShowWindow(window.getNativeHandle(), SW_MINIMIZE);
	SetForegroundWindow(GetDesktopWindow());

	while (!XMsg::IsClose(window))
	{
		XWindow::DelayFps(window,30);

		if (window.hasFocus())
			break;
	}

	//还原
	ShowWindow(window.getNativeHandle(), SW_RESTORE);
	SetForegroundWindow(window.getNativeHandle());

	ImgY.SetAnimationStartValue(-WindowSize.y);
	ImgY.SetAnimation(0, TotalFrame * 2);

	while (!XMsg::IsClose(window))
	{
		XWindow::DelayFps(window,60);

		XSystem::Taskbar::SetTaskBarVisible(false);
		ImgY.UpdateAnimation(XEase::EaseBasic::easeOut, 8);

		XImage::PutImage(temp, 0, ImgY.value,window);

		if (!ImgY.IsAnimation()) break;
	}

	XWindow::SetBackGroundColor(Color(30, 30, 30));
}

#pragma endregion

//插件
void SetExtV(bool v);
void UpdateExt();

//底部工具栏
#pragma region MyRegion

class MainBar
{
	RenderTexture rt;
	bool init = false;

	bool NeedReDraw = true;

	void UnDo()
	{
		Write::UnDo();
	}

	//图标y偏移
	EV IconYOffset[5];
	//图标缩放
	EV IconScale[5];

	//底线坐标
	EV BottomLineX1, BottomLineX2;
	//底线方向
	bool IsLeft = false;
	//底线缩放
	EV BottomLineScale;
	//底线基准长度
	int BottomLineBasicLengh = WindowSize.x * 0.01;

	//缩放
	EV yScale;

	void Ext()
	{
		SetExtV(true);
	}

	//收缩时间
	int exptime = -1;

public:
	EV2 pos, size;

	float CenterX = 0.5, CenterY = 0;

	wstring ToolName[5] =
	{
		L"软笔",L"橡皮擦",L"移动缩放",L"撤销",L"更多插件"
	};

	void Draw(RenWin& window)
	{
		//图标
		static IMAGE Icon[4];
		static IMAGE PenIcon[4];
		
		//图标启动y偏移
		static EV IconStartYOffset[5];
		//启动时间
		static int StartClock = 30;
		if (StartClock > -90) StartClock -= 1;

		//执行初始化
		if (!init)
		{
			init = true;
			NeedReDraw = true;
			
			//设置大小动画
			size.x.SetAnimationStartValue(0);
			size.x.SetAnimation(BasicSize * 5, TotalFrame);
			size.y.SetAnimationStartValue(BasicSize);

			//设置坐标动画
			pos.x.SetAnimationStartValue(WindowSize.x / 2);
			pos.y.SetAnimationStartValue(WindowSize.y);
			pos.y.SetAnimation(WindowSize.y - BasicSize - UISpace, TotalFrame);

			//设置缩放动画
			yScale.SetAnimationStartValue(100);

			CenterX = 0.5;

			//图标初始化
			wstring ImgName[4] = {L"Erase.dll",L"Move.dll",L"Back.dll",L"More.dll" };
			for (int i = 0; i < 4; i++) 
			{
				if (!LoadIcon(Icon[i], ImgPath + L"MainTool\\" + ImgName[i]))
				{
					RightMessage::ShowMessage(L"错误：定位图像MainTool失败，使用默认图标", L"图标加载", RightMessageType_ERROR,true);
				}
			}
			wstring PenIconImgName[4] = { L"Soft.dll",L"Hard.dll",L"Light.dll",L"Leaf.dll" };
			for (int i = 0; i < 4; i++)
			{
				if (!LoadIcon(PenIcon[i], ImgPath + L"MoreTool\\Pen\\" + PenIconImgName[i]))
				{
					RightMessage::ShowMessage(L"错误：定位图像MainTool.PenCap失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
				}
			}

			for (int i = 0; i < 5; i++)
			{
				IconScale[i].SetAnimationStartValue(20);
				IconScale[i].SetAnimation(100, TotalFrame);
				IconStartYOffset[i].SetAnimationStartValue(BasicSize);
				IconStartYOffset[i].SetAnimation(0, TotalFrame * 2 / 3);
			}

			IconYOffset[Tool::ToolCount].SetAnimation(-UISpace / 2, TotalFrame);

			BottomLineBasicLengh = WindowSize.x * 0.01;
			BottomLineScale.SetAnimation(100, TotalFrame);
			BottomLineX1.SetAnimationStartValue(BasicSize / 2 - BottomLineBasicLengh);
			BottomLineX2.SetAnimationStartValue(BasicSize / 2 + BottomLineBasicLengh);
			
		}

		//脏标记
		if(!NeedReDraw)
			NeedReDraw = IsAnimations(
			size.x,size.y,IconScale[4],IconScale[3],IconScale[2],IconScale[1],IconScale[0],
			BottomLineX1,BottomLineX2,BottomLineScale
			);

		//动画更新
		pos.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		pos.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		size.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		size.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		
		//底线动画更新
		if (IsLeft)
		{
			BottomLineX1.UpdateAnimation(XEase::EaseBasic::easeOut, 3);
			if (BottomLineX1.frame > 7) BottomLineX2.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		}
		else
		{
			BottomLineX2.UpdateAnimation(XEase::EaseBasic::easeOut, 3);
			if (BottomLineX2.frame > 7) BottomLineX1.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		}
		//只有在启动动画结束后才执行，图标弹跳动画，底线缩放动画
		if(StartClock <= -45)
		{
			//底线缩放动画更新
			BottomLineScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 3);

			//图标弹跳动画
			for (int i = 0; i < 5; i++)
			{
				if (IconYOffset[i].start < IconYOffset[i].end)
					IconYOffset[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
				else
					IconYOffset[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 3);
			}
		}
		//启动动画，图标缩放动画
		if (StartClock <= 0)
		{
			//图标动画更新，只有在启动时才执行逻辑，提高性能，更新y偏移与缩放
			if (StartClock > -90)
			{
				for (int i = 0; i < 5; i++)
				{
					if (i == 0)
					{
						IconScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
						IconStartYOffset[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
					}
					else if (IconScale[i - 1].frame > IconScale[i - 1].totalframe / 4)
					{
						IconScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
						IconStartYOffset[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
					}
				}
			}
			//启动动画后，只更新缩放
			else
			{
				for (int i = 0; i < 5; i++)
				{
					if (i == 0)
					{
						IconScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
					}
					else if (IconScale[i - 1].frame > IconScale[i - 1].totalframe / 6)
					{
						IconScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
					}
				}
			}
		}
		
		//绘制
		if (NeedReDraw || UpdateUser > 0)
		{
			if (rt.resize(FTU({ size.x.value + 2,size.y.value + 2 })))
			{
				rt.clear(Color::Transparent);

				//绘制底色
				DrawGlassBar(1, 1, size.x.value, size.y.value, RoundSize, rt);

				//绘制图标
				for (int i = 0; i < 5; i++)
				{
					//计算坐标（居中）
					int x = size.x.value / 2 + BasicSize * (i - 2);
					static int y = BasicSize / 2;

					static float ImgScale = (float)IconSize / 512.0;

					if(i > 0)
					{
						if (i != Tool::ToolCount) Icon[i - 1].color.a = NoActiveIconAlpha;

						XImage::PutScaleImage(
							Icon[i - 1],
							1 + x, 1 + y + IconStartYOffset[i].value + IconYOffset[i].value,
							ImgScale * IconScale[i].value / 100.0, ImgScale * IconScale[i].value / 100.0,
							rt, 0.5, 0.5);

						Icon[i - 1].color.a = 255;
					}
					else
					{
						if (i != Tool::ToolCount) PenIcon[Tool::PenCap].color.a = NoActiveIconAlpha;

						XImage::PutScaleImage(
							PenIcon[Tool::PenCap],
							1 + x, 1 + y + IconStartYOffset[i].value + IconYOffset[i].value,
							ImgScale* IconScale[i].value / 100.0, ImgScale* IconScale[i].value / 100.0,
							rt, 0.5, 0.5);

						PenIcon[Tool::PenCap].color.a = 255;
					}
				}

				//绘制文字
				XText::SetFontConfig(Color::White, FONTSIZE * 0.9);
				XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
				for (int i = 0; i < 5; i++)
				{
					if (i == Tool::ToolCount && StartClock < -45) continue;

					//计算坐标（居中）
					int x = size.x.value / 2 + BasicSize * (i - 2);
					static int y = BasicSize * 0.91;

					XText::Xyprintf(1 + x, 1 + y + IconStartYOffset[i].value, ToolName[i],rt);
				}

				//绘制底线，自动识别并矫正位置
				static int LineWidth = WindowSize.x / 150;
				XGraph::LineShape::SetLineWidth(LineWidth);
				if(Tool::ToolCount > 0) XGraph::SetColor(User::MainColor);
				else XGraph::SetColor(Tool::PenColor);
				static int LineY = BasicSize * 0.91;
				int ScaleLengh = BottomLineBasicLengh * (100 - BottomLineScale.value) / 100.0;

				int XOffset = (size.x.value - BasicSize * 5) / 2;
				XGraph::LineShape::Line(
					1 + BottomLineX1.value + ScaleLengh + XOffset, LineY,
					1 + BottomLineX2.value - ScaleLengh + XOffset, LineY, rt);

				rt.display();
			}
		}

		//按键处理
		if(XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			for (int i = 0; i < 5; i++)
			{
				int x = pos.x.value + size.x.value / 2 + BasicSize * (i - 2) - BasicSize / 2- size.x.value * CenterX + 1,
					y = pos.y.value - size.y.value * CenterY + 1, 
					w = BasicSize, h = BasicSize;

				if (XMsg::MouseMsg::IsMouseIn(x, y, w, h))
				{

					//当工具选项不等于当前选项，切换至此，且隐藏更多工具栏
					if (Tool::ToolCount != i && i != 3)
					{
						//更新图标动画
						IconYOffset[i].SetAnimation(-UISpace / 2, TotalFrame);
						IconYOffset[Tool::ToolCount].SetAnimation(0, TotalFrame);
						IconScale[i].SetAnimationStartValue(60);
						IconScale[i].SetAnimation(100, TotalFrame);

						//更新底线动画
						int XOffset = (size.x.value - BasicSize * 5) / 2;
						BottomLineX1.SetAnimation(size.x.value / 2 + BasicSize * (i - 2) - BottomLineBasicLengh - XOffset, TotalFrame / 3);
						BottomLineX2.SetAnimation(size.x.value / 2 + BasicSize * (i - 2) + BottomLineBasicLengh - XOffset, TotalFrame / 3);

						if (i < Tool::ToolCount) IsLeft = true;
						else IsLeft = false;

						//更新值
						Tool::ToolCount = i;

						//隐藏更多工具栏
						Tool::HideMoreBar();

						if (i == 4) Ext();
					}
					//召唤更多选项，或者单独功能
					else
					{
						//图标收缩动画
						IconScale[i].SetAnimationStartValue(60);
						IconScale[i].SetAnimation(100, TotalFrame);

						if(i != 3)
						{
							//底线收缩动画
							BottomLineScale.SetAnimationStartValue(40);
							BottomLineScale.SetAnimation(100, TotalFrame);

							//召唤更多选项
							if (i == 4) Ext();
							else if(i != 2) UpdateMoreBarVisible();
						}
						else
						{
							UnDo();
						}
					}
				}
			}
		}
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseRight))
		{
			for (int i = 0; i < 5; i++)
			{
				int x = pos.x.value + size.x.value / 2 + BasicSize * (i - 2) - BasicSize / 2 - size.x.value * CenterX + 1,
					y = pos.y.value - size.y.value * CenterY + 1,
					w = BasicSize, h = BasicSize;

				if (XMsg::MouseMsg::IsMouseIn(x, y, w, h))
				{
					if(i == 2)
					{
						Write::ResetMove();
						BottomMessage::AddMessage(107, L"已复位", BottomMessageType_INFO, 180);
					}
					if (i == 3)
					{
						Write::EraseAll();

						//发送信息
						if (!BottomMessage::IsMessageNow(102))
						{
							BottomMessage::AddMessage(102, L"已全部擦除", BottomMessageType_SUCCESS, 180);
						}
						else
						{
							BottomMessage::ResetY();
							BottomMessage::UpdateCurretnMessageTime(180);
						}

						SetTool(0);
						XMsg::ClearMsg();
						XMsg::SetSleepTime(10);
					}
				}
			}
		}

		//坐标检测
		if (XMsg::MouseMsg::IsMouseIn(
			pos.x.value - size.x.value * CenterX, pos.y.value - size.y.value * CenterY, size.x.value, size.y.value))
		{
			Tool::IsInBar = true;
		}

		//自动回放检测
		if (exptime > -1)
		{
			exptime -= 1;
			if (exptime == 0)
			{
				static int ExpPos = WindowSize.y - BasicSize - UISpace;
				pos.y.SetAnimation(ExpPos, TotalFrame);

				yScale.SetAnimationStartValue(ScaleStrgengh);
				yScale.SetAnimation(100, TotalFrame);
			}
		}

		if(pos.y.frame > MinFrame) yScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		Sprite s(rt.getTexture());
		
		s.setPosition({ pos.x.value - 1- size.x.value * CenterX,pos.y.value - 1 - size.y.value * CenterY });
		s.setColor(Color(255, 255, 255, UIAlpha));
		s.setScale(Vector2f(1, yScale.value / 100.0));
		window.draw(s);

		NeedReDraw = false;
	}
	void Update()
	{
		NeedReDraw = true;
	}
	void SetTool(int index)
	{
		//更新图标动画
		IconYOffset[index].SetAnimation(-UISpace / 2, TotalFrame);
		IconYOffset[Tool::ToolCount].SetAnimation(0, TotalFrame);
		IconScale[index].SetAnimationStartValue(60);
		IconScale[index].SetAnimation(100, TotalFrame);

		//更新底线动画
		int XOffset = (size.x.value - BasicSize * 5) / 2;
		BottomLineX1.SetAnimation(size.x.value / 2 + BasicSize * (index - 2) - BottomLineBasicLengh - XOffset, TotalFrame / 3);
		BottomLineX2.SetAnimation(size.x.value / 2 + BasicSize * (index - 2) + BottomLineBasicLengh - XOffset, TotalFrame / 3);

		if (index < Tool::ToolCount) IsLeft = true;
		else IsLeft = false;

		//更新值
		Tool::ToolCount = index;

		//隐藏更多工具栏
		Tool::HideMoreBar();
	}
	void UpdatePenCap()
	{
		IconScale[0].SetAnimationStartValue(50);
		IconScale->SetAnimation(100, TotalFrame);
	}

	void exp()
	{
		exptime = EXP_TIME;

		static int ExpPos = WindowSize.y - UISpace - BasicSize * 0.1;
		if (pos.y.IsAnimation() || pos.y.value >= ExpPos) return;

		pos.y.SetAnimation(ExpPos, TotalFrame);
	}
};
static MainBar MainToolBar;

#pragma endregion

//更多工具栏
#pragma region MyRegion

//自定义颜色
#pragma region MyRegion

static Vector2i GetMin(Vector2i size, float Scale)
{
	int sizeReturn = min(size.x, size.y);
	sizeReturn *= Scale;
	return Vector2i{ sizeReturn ,sizeReturn };
}

static VertexArray createGradientRectangle(
	const Vector2f& position,
	const Vector2f& size,
	const Color& topColor,
	const Color& bottomColor)
{
	// 使用 Triangles，需要6个顶点（2个三角形）
	VertexArray vertices(PrimitiveType::Triangles, 6);

	// 定义矩形的四个角点
	Vector2f topLeft = position;
	Vector2f topRight = position + Vector2f(size.x, 0);
	Vector2f bottomRight = position + size;
	Vector2f bottomLeft = position + Vector2f(0, size.y);

	// 第一个三角形 (左上 -> 右上 -> 左下)
	vertices[0].position = topLeft;
	vertices[0].color = topColor;      // 顶部颜色

	vertices[1].position = topRight;
	vertices[1].color = topColor;      // 顶部颜色

	vertices[2].position = bottomLeft;
	vertices[2].color = bottomColor;   // 底部颜色

	// 第二个三角形 (右上 -> 右下 -> 左下)
	vertices[3].position = topRight;
	vertices[3].color = topColor;      // 顶部颜色

	vertices[4].position = bottomRight;
	vertices[4].color = bottomColor;   // 底部颜色

	vertices[5].position = bottomLeft;
	vertices[5].color = bottomColor;   // 底部颜色

	return vertices;
}

static void DrawGradientRect(int x, int y, int w, int h, Color TopColor, Color BottomColor, RenderWindow& window)
{
	// 绘制渐变矩形
	auto gradient = createGradientRectangle(
		Vector2f(x, y),
		Vector2f(w, h),
		TopColor,   // 顶部
		BottomColor    // 底部
	);

	window.draw(gradient);
}

// 颜色选择窗口
Color ChooseColorWindow(const Color& cancelColor, RenderWindow& window)
{
	Color ReturnColor = cancelColor;
	Color ChooseColorA = cancelColor;

	static Vector2i LastMousePos = { 0,0 };

	IMAGE ColorChooseImage;
	if (!XImage::NewImage(ColorChooseImage, ImgPath + L"\\Color.dll"))
	{
		MsgS WrongMsg;
		WrongMsg.audio = 2;
		WrongMsg.text = "初始化控件错误";
		WrongMsg.title = "错误";
		WrongMsg.buttons = { "确定" };
		WrongMsg.Ico = ICOTYPE_ERROR;
		Message::ShowMessage(WrongMsg,L"MiuBarrd");

		return cancelColor;
	}	

	RenderWindow ColorChooseWindow;
	XWindow::CreateGraphWindow(ColorChooseWindow,-1,-1,ScreenSize.x / 3, ScreenSize.y / 2, L"选择颜色", Style::Default);

	HWND hWnd = ColorChooseWindow.getNativeHandle();
	SetWindowLongPtr(hWnd, GWL_EXSTYLE,
		WS_EX_TOPMOST);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	ColorChooseWindow.requestFocus();

	Image icon;
	icon.loadFromFile(ImgPath + L"Icon.dll");
	ColorChooseWindow.setIcon(icon);

	ColorChooseWindow.setMinimumSize(Vector2u(ScreenSize.x / 3, ScreenSize.y / 2));

	XWindow::DWM::SetWindowBackType(ColorChooseWindow, BACKTYPE_BLUR);
	XWindow::DWM::ExtendIntoClientArea(ColorChooseWindow, -1, -1, -1, -1);
	XWindow::DWM::SetWindowDarkMode(ColorChooseWindow, true);

	SetWindowPos(ColorChooseWindow.getNativeHandle(), HWND_TOPMOST, (ScreenSize.x - ColorChooseWindow.getSize().x) / 2,
		(ScreenSize.y - ColorChooseWindow.getSize().y) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	Vector2i ChooseArrow = LastMousePos;

	int BrightPrecent = 100;
	bool isExit = false;

	const wstring buttonText[2] = { L"选取",L"取消" };

	int buttonWidth = ScreenSize.x / 14;
	int buttonHeight = ScreenSize.y / 27;

	while (1)
	{
		XWindow::DelayFps(ColorChooseWindow,30);
		ColorChooseWindow.clear(Color::Transparent);

		Vector2i WindowSizeTemp = XWindow::GetWindowSize(ColorChooseWindow);

		XGraph::SetFillColor(Color(10, 10, 10, 100));
		XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, WindowSizeTemp.x, WindowSizeTemp.y, ColorChooseWindow);

		Vector2i ColorImgSize = GetMin(WindowSizeTemp, 0.7);
		XImage::PutScaleImage(ColorChooseImage,
			ScreenSize.x / 100, ScreenSize.y / 70 + ScreenSize.y / 15,
			ColorImgSize.x / 512.0, ColorImgSize.y / 512.0, ColorChooseWindow);

		Vector2i imgPos(ScreenSize.x / 100, ScreenSize.y / 70 + ScreenSize.y / 15);

		if (IsIconic(ColorChooseWindow.getNativeHandle()))
		{
			ShowWindow(ColorChooseWindow.getNativeHandle(), SW_RESTORE);
		}

		if (XMsg::MouseMsg::IsMousePress(VK::MouseLeft))
		{
			Vector2i MP = XMsg::MouseMsg::GetMousePosWindow();

			// 检查是否点击在图片区域内
			if (MP.x >= imgPos.x - ScreenSize.x / 100 && MP.x <= imgPos.x + ColorImgSize.x + ScreenSize.x / 100 &&
				MP.y >= imgPos.y - ScreenSize.x / 100 && MP.y <= imgPos.y + ColorImgSize.y + ScreenSize.x / 100)
			{
				if (MP.x < imgPos.x) MP.x = imgPos.x;
				if (MP.x > imgPos.x + ColorImgSize.x) MP.x = imgPos.x + ColorImgSize.x;
				if (MP.y < imgPos.y) MP.y = imgPos.y;
				if (MP.y > imgPos.y + ColorImgSize.y) MP.y = imgPos.y + ColorImgSize.y;

				// 将窗口坐标映射到图片像素坐标
				// 先取出纹理尺寸
				Vector2u srcSize = ColorChooseImage.texture.getSize();
				if (srcSize.x > 0 && srcSize.y > 0)
				{
					float fx = (MP.x - imgPos.x) / static_cast<float>(ColorImgSize.x);
					float fy = (MP.y - imgPos.y) / static_cast<float>(ColorImgSize.y);

					unsigned int ix = static_cast<unsigned int>(floorf(fx * srcSize.x));
					unsigned int iy = static_cast<unsigned int>(floorf(fy * srcSize.y));
					if (ix >= srcSize.x) ix = srcSize.x - 1;
					if (iy >= srcSize.y) iy = srcSize.y - 1;

					// 通过 copyToImage 读取像素
					Image img = ColorChooseImage.texture.copyToImage();
					Color sc = img.getPixel(Vector2u(ix, iy));

					ChooseArrow = MP; LastMousePos = MP;

					ChooseColorA = Color(sc.r, sc.g, sc.b, sc.a);
					ReturnColor = XColor::AdjustColorBright(ChooseColorA, BrightPrecent);
				}
			}

			if (XMsg::MouseMsg::IsMouseIn(ScreenSize.x / 20 + ColorImgSize.x, ScreenSize.y / 70 + ScreenSize.y / 15 - ScreenSize.x / 100,
				ScreenSize.x / 75, ColorImgSize.y + ScreenSize.x / 50))
			{
				if (MP.y < ScreenSize.y / 70 + ScreenSize.y / 15) MP.y = ScreenSize.y / 70 + ScreenSize.y / 15;
				if (MP.y > ScreenSize.y / 70 + ScreenSize.y / 15 + ColorImgSize.y) MP.y = ScreenSize.y / 70 + ScreenSize.y / 15 + ColorImgSize.y;
				BrightPrecent = 100 - (MP.y - (ScreenSize.y / 70 + ScreenSize.y / 15)) * 100 / ColorImgSize.y;

				ReturnColor = XColor::AdjustColorBright(ChooseColorA, BrightPrecent);
			}

		}

		XGraph::SetColor(Color(150, 150, 150));
		XGraph::LineShape::SetLineWidth(ScreenSize.x / 480);
		XGraph::LineShape::Line(ChooseArrow.x - ScreenSize.x / 100, ChooseArrow.y, ChooseArrow.x + ScreenSize.x / 100, ChooseArrow.y, ColorChooseWindow);
		XGraph::LineShape::Line(ChooseArrow.x, ChooseArrow.y - ScreenSize.x / 100, ChooseArrow.x, ChooseArrow.y + ScreenSize.x / 100, ColorChooseWindow);


		XGraph::SetFillColor(ReturnColor);
		int width = ScreenSize.x / 50;
		XGraph::RectangleShape::FillRect_WithoutBorder(
			ScreenSize.x / 50 + ColorImgSize.x, ScreenSize.y / 70 + ScreenSize.y / 15,
			width, ColorImgSize.y, ColorChooseWindow);

		DrawGradientRect(ScreenSize.x / 20 + ColorImgSize.x, ScreenSize.y / 70 + ScreenSize.y / 15,
			width * 2 / 3, ColorImgSize.y, XColor::AdjustColorBright(ChooseColorA, 100), XColor::AdjustColorBright(ChooseColorA, 0),
			ColorChooseWindow);

		XGraph::SetFillColor(Color(100, 100, 100));
		XGraph::CircleShape::FillCircle_WithoutBorder(
			ScreenSize.x / 20 + ColorImgSize.x + width / 3, ScreenSize.y / 70 + ScreenSize.y / 15 + ColorImgSize.y * (100 - BrightPrecent) / 100,
			width / 3, ColorChooseWindow);
		XGraph::SetFillColor(Color(255, 255, 255));
		XGraph::CircleShape::FillCircle_WithoutBorder(
			ScreenSize.x / 20 + ColorImgSize.x + width / 3, ScreenSize.y / 70 + ScreenSize.y / 15 + ColorImgSize.y * (100 - BrightPrecent) / 100,
			width / 5, ColorChooseWindow);

		if (XMsg::IsClose(ColorChooseWindow))
		{
			ReturnColor = cancelColor;
			break;
		}
		if (isExit)
		{
			break;
		}
		if (!ColorChooseWindow.hasFocus())
		{
			ColorChooseWindow.requestFocus();
		}

		for (int i = 0; i < 2; i++)
		{
			if (i == 0)
			{
				XGraph::SetFillColor(XColor::DarkColor(User::MainColor, 0.3));
				XGraph::LineShape::SetLineWidth(ScreenSize.x / 500);
				XGraph::RectangleShape::FillRoundRect_WithoutBorder(WindowSizeTemp.x - buttonWidth * (i + 1) * 6 / 5, WindowSizeTemp.y - buttonHeight * 7 / 5,
					buttonWidth, buttonHeight, buttonHeight / 3, ColorChooseWindow);
			}
			else
			{
				XGraph::SetColor(XColor::DarkColor(XColor::GrayColor(User::MainColor, 0.6), 0.5));
				XGraph::LineShape::SetLineWidth(ScreenSize.x / 500);
				XGraph::RectangleShape::RoundRect(WindowSizeTemp.x - buttonWidth * (i + 1) * 6 / 5, WindowSizeTemp.y - buttonHeight * 7 / 5,
					buttonWidth, buttonHeight, buttonHeight / 3, ColorChooseWindow);
			}

			if (i == 0) XText::SetFontColor(Color::White);
			else XText::SetFontColor(Color(200, 200, 200));
			XText::SetFontSize(ScreenSize.x / 100);
			XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
			XText::SetFontColor(Color::White);
			XText::Xyprintf(WindowSizeTemp.x - buttonWidth * (i + 1) * 6 / 5 + buttonWidth / 2,
				WindowSizeTemp.y - buttonHeight * 7 / 5 + buttonHeight / 2, buttonText[i], ColorChooseWindow);

			if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft)
				&&
				XMsg::MouseMsg::IsMouseIn(WindowSizeTemp.x - buttonWidth * (i + 1) * 6 / 5, WindowSizeTemp.y - buttonHeight * 7 / 5,
					buttonWidth, buttonHeight))
			{
				if (i == 1)
				{
					ReturnColor = cancelColor;
				}
				isExit = true;
				break;
			}
		}

		for (int i = 0; i < 3; i++)
		{
			int W = WindowSizeTemp.x - (ScreenSize.x / 20 + ColorImgSize.x + width * 2 / 3) - ScreenSize.x / 50;
			int X = ScreenSize.x / 20 + ColorImgSize.x + width * 2 / 3 + ScreenSize.x / 100;
			int H = ScreenSize.y / 25;
			int Y = ScreenSize.y / 70 + ScreenSize.y / 15 + i * H * 13 / 10;

			XGraph::SetFillColor(Color(70, 70, 70));
			XGraph::SetColor(ReturnColor);

			XGraph::RectangleShape::FillRoundRect_WithoutBorder(X, Y, W, H, H / 5, ColorChooseWindow);
			XGraph::LineShape::SetLineWidth(ScreenSize.x / 480);
			XGraph::LineShape::Line(X + H / 10, Y + H - ScreenSize.x / 960, X + W - H / 10, Y + H - ScreenSize.x / 960, ColorChooseWindow);

			XText::SetFontSize(ScreenSize.x / 100);
			XText::SetFontColor(Color::White);
			XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);

			wstring text;

			if (i == 0) text = L"R：" + to_wstring(ReturnColor.r);
			if (i == 1) text = L"G：" + to_wstring(ReturnColor.g);
			if (i == 2) text = L"B：" + to_wstring(ReturnColor.b);
			XText::Xyprintf(X + W / 2, Y + H / 2, text, ColorChooseWindow);
		}
	}

	XMsg::ResetCloseMsg();

	XMsg::ClearMsg();
	XMsg::UpdateMsg(window);
	XMsg::ClearMsg();

	ColorChooseWindow.close();
	return ReturnColor;
}

#pragma endregion

class MoreBar
{
	wstring PenCapName[4] = { L"软笔",L"硬粉笔",L"荧光笔",L"柳叶笔" };

	EV2 pos, size;

	RenderTexture rt;
	bool init = false;

	bool PerNeedRedraw = false;
	bool NeedReDraw = true;

	bool ETempLayer = false;

	Color ChooseColor(Color resetColor, RenderWindow& window)
	{
		Color c = ChooseColorWindow(resetColor, window);
		XWindow::SetBackGroundColor(Color(30, 30, 30));

		PerNeedRedraw = true;
		NeedReDraw = true;

		return c;
	}

	void EraseAll()
	{
		Write::EraseAll();
		//发送信息
		if(!BottomMessage::IsMessageNow(102))
		{
			BottomMessage::AddMessage(102, L"已全部擦除", BottomMessageType_SUCCESS, 180);
		}
		else
		{
			BottomMessage::ResetY();
			BottomMessage::UpdateCurretnMessageTime(180);
		}

		MainToolBar.SetTool(0);
		XMsg::ClearMsg();
		XMsg::SetSleepTime(10);
	}
	void EnableTempLayer()
	{
		Write::EnableTempLayer(!ETempLayer);
		ETempLayer = !ETempLayer;
	}

	//绘制笔更多设置
	void DrawTool0(RenWin& window)
	{
		//笔帽图标
		static IMAGE Icon[4];
		//笔帽偏移y
		static EV PenCapYOffset[4];
		//笔帽图标缩放
		static EV PenCapScale[4];
		//笔帽底线缩放
		static EV BottomLineScale;
		//笔帽底线基准坐标
		static int BasicBottomLineLengh = WindowSize.x / 100;
		//笔粗细提示缩放
		static EV PenSizeRectScale;
		//笔颜色提示缩放
		static EV PenColorRectScale;
		//笔颜色自定义图标
		static IMAGE MoreColorIcon;

		//初始化
		static bool Init = false;
		if (!Init)
		{
			//图标初始化
			wstring ImgName[4] = { L"Soft.dll",L"Hard.dll",L"Light.dll",L"Leaf.dll" };
			for (int i = 0; i < 4; i++)
			{
				if (!LoadIcon(Icon[i], ImgPath + L"MoreTool\\Pen\\" + ImgName[i]))
				{
					RightMessage::ShowMessage(L"错误：定位图像MoreTool.Pen失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
				}

				PenCapScale[i].SetAnimation(100,TotalFrame);
			}

			if (!LoadIcon(MoreColorIcon, ImgPath + L"MoreTool\\Pen\\MoreColor.dll"))
			{
				RightMessage::ShowMessage(L"错误：定位图像MoreTool.Pen.MoreColor失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
			}

			Init = true;

			PenCapYOffset[Tool::PenCap].SetAnimation(-UISpace / 2, TotalFrame);

			BottomLineScale.SetAnimation(100, TotalFrame);

			PenSizeRectScale.SetAnimation(100, TotalFrame);

			PenColorRectScale.SetAnimation(100, TotalFrame);
		}

		//脏标记
		if (!NeedReDraw)
		{
			NeedReDraw = IsAnimations(
				BottomLineScale,PenSizeRectScale,PenColorRectScale,
				PenCapYOffset[0],PenCapYOffset[1],PenCapYOffset[2],PenCapYOffset[3],
				PenCapScale[0],PenCapScale[1],PenCapScale[2],PenCapScale[3]
				);
		}

		if(pos.y.frame > TotalFrame / 3)
		{
			//更新动画
			for (int i = 0; i < 4; i++)
			{
				PenCapYOffset[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
				if(i == 0) PenCapScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
				else if(PenCapScale[i - 1].frame > TotalFrame / 10) PenCapScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
			}
		}
		BottomLineScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
		PenSizeRectScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
		PenColorRectScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);

		//绘制
		if (NeedReDraw || UpdateUser > 0)
		{
			if (rt.resize(FTU({ size.x.value + 2,size.y.value + 2 })))
			{
				rt.clear(Color::Transparent);

				//绘制底色
				DrawGlassBar(1, 1, size.x.value, size.y.value, RoundSize, rt);

				//绘制笔帽图标
				for (int i = 0; i < 4; i++)
				{
					//绘制坐标（居中）
					int x = 1 + BasicSize / 2 + BasicSize * i / 1.5;
					static int y = 1 + BasicSize / 2;

					static float ImgScale = IconSize * 0.8 / 512.0;

					float Scale = ImgScale * PenCapScale[i].value / 100.0;

					if (Tool::PenCap != i) Icon[i].color.a = NoActiveIconAlpha;
					XImage::PutScaleImage(Icon[i], x, y + PenCapYOffset[i].value, Scale, Scale, rt, 0.5, 0.5);
					Icon[i].color.a = 255;
				}

				//绘制笔帽文字和底线
				XText::SetFontConfig(Color::White, FONTSIZE * 0.9);
				XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
				for (int i = 0; i < 4; i++)
				{
					//绘制坐标（居中）
					int x = 1 + BasicSize / 2 + BasicSize * i / 1.5;
					static int y = 1 + BasicSize * 0.8;

					//绘制底线
					if (i == Tool::PenCap)
					{
						static int lw = WindowSize.x / 150;
						XGraph::LineShape::SetLineWidth(lw);
						XGraph::SetColor(User::MainColor);
						int Lengh = BasicBottomLineLengh * BottomLineScale.value / 100.0;
						XGraph::LineShape::Line(x - Lengh, y, x + Lengh, y, rt);
					}
					//绘制文本
					else
					{
						XText::Xyprintf(1 + x, 1 + y, PenCapName[i], rt);
					}
				}

				//绘制笔粗细
				XGraph::SetFillColor(Tool::PenColor);
				static int x[2] = { BasicSize * 3.2,BasicSize * 3.7 };
				static int LineWidth = WindowSize.x / 500;
				static int ROUNDSIZE = WindowSize.x / 300;
				static int RectSize = WindowSize.x / 50;

				int PenSizeRectScaleSize = RectSize * (100 - PenSizeRectScale.value) / 200.0;

				for (int i = 0; i < 2; i++)
				{
					int y = BasicSize * 0.3;
					XGraph::CircleShape::FillCircle_WithoutBorder(x[i], y, PerSetPenSize[i], rt);

					//设置标注
					if (Tool::Tool::PenSize == PerSetPenSize[i])
					{
						XGraph::SetColor(User::MainColor);
						
						XGraph::LineShape::SetLineWidth(LineWidth);
						XGraph::RectangleShape::RoundRect(
							x[i] - RectSize / 2 + PenSizeRectScaleSize, y - RectSize / 2 + PenSizeRectScaleSize, 
							RectSize - PenSizeRectScaleSize * 2, RectSize - PenSizeRectScaleSize * 2, ROUNDSIZE, rt);
					}
				}
				for (int i = 0; i < 2; i++)
				{
					int y = BasicSize * 0.7;
					XGraph::CircleShape::FillCircle_WithoutBorder(x[i], y, PerSetPenSize[i + 2], rt);

					//设置标注
					if (Tool::Tool::PenSize == PerSetPenSize[i + 2])
					{
						XGraph::SetColor(User::MainColor);

						XGraph::LineShape::SetLineWidth(LineWidth);
						XGraph::RectangleShape::RoundRect(
							x[i] - RectSize / 2 + PenSizeRectScaleSize, y - RectSize / 2 + PenSizeRectScaleSize,
							RectSize - PenSizeRectScaleSize * 2, RectSize - PenSizeRectScaleSize * 2, ROUNDSIZE, rt);
					}
				}

				//绘制颜色表
				static int ColorRectSize = WindowSize.x / 55;
				for (int i = 0; i < 8; i++)
				{
					int x = BasicSize * 4.3 + ColorRectSize * 1.2 * i;
					int y = BasicSize * 0.3 - ColorRectSize / 2;
					Color color = PersetPenColor[i];
					XGraph::SetFillColor(color);

					if(Tool::PenColor == color)
					{
						XGraph::RectangleShape::FillRoundRect_WithoutBorder(
							x + ColorRectSize * 0.1, y + ColorRectSize * 0.1, ColorRectSize * 0.8, ColorRectSize * 0.8, ROUNDSIZE, rt);

						static int lw = WindowSize.x / 500;
						XGraph::SetColor(User::MainColor);
						XGraph::LineShape::SetLineWidth(lw);
						int RectScaleSize = ColorRectSize * (100 - PenColorRectScale.value) / 200.0;
						XGraph::RectangleShape::RoundRect(
							x + RectScaleSize, y + RectScaleSize, 
							ColorRectSize - RectScaleSize * 2, ColorRectSize - RectScaleSize * 2, ROUNDSIZE,rt);
					}
					else
					{
						XGraph::RectangleShape::FillRoundRect_WithoutBorder(x, y, ColorRectSize, ColorRectSize, ROUNDSIZE, rt);
					}
				}
				for (int i = 0; i < 7; i++)
				{
					int x = BasicSize * 4.3 + ColorRectSize * 1.2 * i;
					int y = BasicSize * 0.7 - ColorRectSize / 2;
					Color color = PersetPenColor[8 + i];
					XGraph::SetFillColor(color);

					if (Tool::PenColor == color)
					{
						XGraph::RectangleShape::FillRoundRect_WithoutBorder(
							x + ColorRectSize * 0.1, y + ColorRectSize * 0.1, ColorRectSize * 0.8, ColorRectSize * 0.8, ROUNDSIZE, rt);

						static int lw = WindowSize.x / 500;
						XGraph::SetColor(User::MainColor);
						XGraph::LineShape::SetLineWidth(lw);
						int RectScaleSize = ColorRectSize * (100 - PenColorRectScale.value) / 200.0;
						XGraph::RectangleShape::RoundRect(
							x + RectScaleSize, y + RectScaleSize,
							ColorRectSize - RectScaleSize * 2, ColorRectSize - RectScaleSize * 2, ROUNDSIZE, rt);
					}
					else
					{
						XGraph::RectangleShape::FillRoundRect_WithoutBorder(x, y, ColorRectSize, ColorRectSize, ROUNDSIZE, rt);
					}
				}
				//绘制自定义颜色
				int ImgX = BasicSize * 4.3 + ColorRectSize * 1.2 * 7;
				int ImgY = BasicSize * 0.7 - ColorRectSize / 2;
				XGraph::SetFillColor(Color(30, 30, 30));
				XGraph::RectangleShape::FillRoundRect_WithoutBorder(ImgX, ImgY, ColorRectSize, ColorRectSize, ROUNDSIZE, rt);
				static float ImgScale = ColorRectSize / 512.0;
				XImage::PutScaleImage(MoreColorIcon,ImgX,ImgY, ImgScale, ImgScale,rt);

				rt.display();
			}
		}

		//按键
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			int XOffset = size.x.value * CenterX;

			//笔帽切换
			for (int i = 0; i < 4; i++)
			{
				//绘制坐标（居中）
				int x = 1 + BasicSize / 2 + BasicSize * i / 1.5- XOffset;
				static int y = 1 + BasicSize / 2;

				static int ImgSize = IconSize * 0.8;

				if (XMsg::MouseMsg::IsMouseIn(pos.x.value + x - ImgSize / 2, pos.y.value + y - ImgSize / 2, ImgSize, ImgSize))
				{

					if (Tool::PenCap == i)
					{
						PenCapYOffset[i].SetAnimationStartValue(0);
						PenCapYOffset[i].SetAnimation(-UISpace / 2, TotalFrame);
						
					}
					else
					{
						PenCapYOffset[Tool::PenCap].SetAnimation(0, TotalFrame);
						Tool::PenCap = i;
						PenCapYOffset[Tool::PenCap].SetAnimation(-UISpace / 2, TotalFrame);

						MainToolBar.ToolName[0] = PenCapName[i];
						MainToolBar.Update();
						MainToolBar.UpdatePenCap();
					}

					BottomLineScale.SetAnimationStartValue(20);
					BottomLineScale.SetAnimation(100, TotalFrame);
					PenCapScale[Tool::PenCap].SetAnimationStartValue(40);
					PenCapScale[Tool::PenCap].SetAnimation(100, TotalFrame);
				}
			}

			//笔大小切换
			int x[2] = { BasicSize * 3.2 - XOffset + pos.x.value,BasicSize * 3.7 - XOffset + pos.x.value };
			static int RectSize = WindowSize.x / 50;
			for (int i = 0; i < 2; i++)
			{
				int y = BasicSize * 0.3;

				if (XMsg::MouseMsg::IsMouseIn(x[i] - RectSize / 2, y - RectSize / 2 + pos.y.value, RectSize, RectSize))
				{
					Tool::Tool::PenSize = PerSetPenSize[i];

					PenSizeRectScale.SetAnimationStartValue(30);
					PenSizeRectScale.SetAnimation(100, TotalFrame);
				}
			}
			for (int i = 0; i < 2; i++)
			{
				int y = BasicSize * 0.7;

				if (XMsg::MouseMsg::IsMouseIn(x[i] - RectSize / 2, y - RectSize / 2 + pos.y.value, RectSize, RectSize))
				{
					Tool::Tool::PenSize = PerSetPenSize[i + 2];

					PenSizeRectScale.SetAnimationStartValue(30);
					PenSizeRectScale.SetAnimation(100, TotalFrame);
				}
			}

			//笔颜色切换
			static int ColorRectSize = WindowSize.x / 55;
			for (int i = 0; i < 8; i++)
			{
				int x = BasicSize * 4.3 + ColorRectSize * 1.2 * i - XOffset;
				int y = BasicSize * 0.3 - ColorRectSize / 2;
				Color color = PersetPenColor[i];

				if (XMsg::MouseMsg::IsMouseIn(pos.x.value + x, pos.y.value + y, ColorRectSize, ColorRectSize))
				{
					Tool::PenColor = PersetPenColor[i];

					MainToolBar.Update();

					PenColorRectScale.SetAnimationStartValue(30);
					PenColorRectScale.SetAnimation(100, TotalFrame);
				}
			}
			for (int i = 0; i < 8; i++)
			{
				int x = BasicSize * 4.3 + ColorRectSize * 1.2 * i - XOffset;
				int y = BasicSize * 0.7 - ColorRectSize / 2;
				if(i < 7)
				{
					Color color = PersetPenColor[8 + i];

					if (XMsg::MouseMsg::IsMouseIn(pos.x.value + x, pos.y.value + y, ColorRectSize, ColorRectSize))
					{
						Tool::PenColor = PersetPenColor[8 + i];

						MainToolBar.Update();

						PenColorRectScale.SetAnimationStartValue(30);
						PenColorRectScale.SetAnimation(100, TotalFrame);
					}
				}
				else
				{
					if (XMsg::MouseMsg::IsMouseIn(pos.x.value + x, pos.y.value + y, ColorRectSize, ColorRectSize))
					{
						Tool::PenColor = ChooseColor(Tool::PenColor,window);

						MainToolBar.Update();
					}
				}
			}
		}
	}

	//绘制橡皮擦更多设置
	void DrawTool1(RenWin& window)
	{
		//图标
		static IMAGE Icon[2];
		//图标缩放
		static EV IconScale[2];

		//初始化
		static bool init = false;
		if (!init)
		{
			wstring ImgName[2] = { L"EraseAll.dll",L"TempLayer.dll" };

			for (int i = 0; i < 2; i++)
			{
				if (!LoadIcon(Icon[i], ImgPath + L"MoreTool\\Erase\\" + ImgName[i]))
				{
					RightMessage::ShowMessage(L"错误：定位图像MoreTool.Erase失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
				}

				IconScale[i].SetAnimation(100,TotalFrame);
			}

			NeedReDraw = true;
			init = true;
		}

		//脏标记
		if (!NeedReDraw)
		{
			NeedReDraw = IsAnimations(
				IconScale[0],IconScale[1] );
		}

		//更新动画
		if(pos.y.frame > TotalFrame / 3)
		{
			IconScale[0].UpdateAnimation(XEase::EaseBasic::easeOutBack, 3);
			if (IconScale[0].frame > TotalFrame / 6) IconScale[1].UpdateAnimation(XEase::EaseBasic::easeOutBack, 3);
		}


		//绘制
		if (NeedReDraw || UpdateUser > 0)
		{
			if (rt.resize(FTU({ size.x.value + 2,size.y.value + 2 })))
			{
				rt.clear(Color::Transparent);

				//绘制底色
				DrawGlassBar(1, 1, size.x.value, size.y.value, RoundSize, rt);

				//绘制图标
				for (int i = 0; i < 2; i++)
				{
					static float Scale = IconSize / 512.0;

					int x = BasicSize * 3.5 + i * BasicSize;
					static int y = BasicSize * 0.45;
					float ImgScale = Scale * IconScale[i].value / 100.0;

					if (i == 1)
					{
						if (ETempLayer) Icon[i].color = User::MainColor;
						else Icon[i].color = Color::White;
					}
					XImage::PutScaleImage(Icon[i], x, y, ImgScale, ImgScale, rt, 0.5, 0.5);
				}

				//绘制文字
				for (int i = 0; i < 2; i++)
				{
					static int FontSize = FONTSIZE * 0.8;

					static wstring text[2] = { L"全部擦除",L"草稿层" };
					XText::SetFontConfig(Color::White, FontSize);
					XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
					int x = BasicSize * 3.5 + i * BasicSize;
					static int y = BasicSize * 0.9;
					XText::Xyprintf(x, y, text[i], rt);
				}

				//绘制大小调整
				static int lw = WindowSize.x / 250;
				XGraph::LineShape::SetLineWidth(lw);
				static int SizeX1 = BasicSize * 0.5, SizeY = BasicSize * 0.45, SizeX2 = BasicSize * 2.5;
				XGraph::LineShape::Line(SizeX1, SizeY, SizeX2, SizeY,rt);
				//圆球
				static int RoundSize = WindowSize.x / 150;
				static int ll = SizeX2 - SizeX1;
				int Basicll = ll * Write::EraseSize / 100;
				XGraph::SetFillColor(Color::White);
				XGraph::CircleShape::FillCircle_WithoutBorder(SizeX1 +  Basicll, SizeY, RoundSize, rt);
				//文字
				static int FontSize = FONTSIZE * 0.8;
				XText::SetFontConfig(Color::White, FontSize);
				XText::SetFontAdjust(ADJUST_LEFT, ADJUST_CENTER);
				XText::Xyprintf(SizeX1, BasicSize * 0.9, L"橡皮擦大小", rt);
				XText::SetFontAdjust(ADJUST_RIGHT, ADJUST_CENTER);
				XText::Xyprintf(SizeX2, BasicSize * 0.9, to_wstring(Write::EraseSize), rt);

				rt.display();
			}
		}

		//按键
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			for (int i = 0; i < 2; i++)
			{
				int x = BasicSize * 3.5 + i * BasicSize;
				static int y = BasicSize * 0.45;

				if (XMsg::MouseMsg::IsMouseIn(
					pos.x.value + x - IconSize / 2 - size.x.value * CenterX, 
					pos.y.value + y - IconSize / 2 - size.y.value * CenterY,
					IconSize, IconSize))
				{
					//全部擦除
					if (i == 0)
					{
						EraseAll();
					}
					//启用/禁用草稿层
					if (i == 1)
					{
						EnableTempLayer();
						NeedReDraw = PerNeedRedraw = true;
					}

					IconScale[i].SetAnimationStartValue(40);
					IconScale[i].SetAnimation(100, TotalFrame);
				}
			}
		}
		if (XMsg::KeyMsg::Keystate(VK::MouseLeft) || XMsg::TouchMsg::IsTouching())
		{
			static int SizeX1 = BasicSize * 0.45, SizeY1 = BasicSize * 0.2, SizeX2 = BasicSize * 2.1, SizeY2 = BasicSize * 0.5;
			if (XMsg::MouseMsg::IsMouseIn(
				pos.x.value - size.x.value * CenterX + SizeX1, pos.y.value - size.y.value * CenterY + SizeY1, SizeX2, SizeY2))
			{
				int v = XMsg::MouseMsg::GetMousePosWindow().x;
				v = v - (pos.x.value - size.x.value * CenterX + BasicSize * 0.5);

				static int ll = BasicSize * 2;
				Write::EraseSize = v * 100 / ll;
				if (Write::EraseSize < 1) Write::EraseSize = 1;
				if (Write::EraseSize > 100) Write::EraseSize = 100;

				NeedReDraw = PerNeedRedraw = true;
			}
		}
	}

public:

	bool Visible = false;

	float CenterX = 0.5, CenterY = 0;

	void Draw(RenWin& window)
	{
		if (!NeedReDraw)
		{
			NeedReDraw = IsAnimations(
				size.x
				);
		}

		//动画更新
		pos.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		pos.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		size.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		size.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		if (!Visible)
		{
			if (!pos.y.IsAnimation())
			{
				pos.y.SetAnimationStartValue(WindowSize.y);
			}
		}

		//如果在窗口外则不绘制
		if (pos.y.value > WindowSize.y - BasicSize * 1.5) return;

		//自主收回
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			if (!XMsg::MouseMsg::IsMouseIn(
				pos.x.value - CenterX * size.x.value, pos.y.value - CenterY * size.y.value,
				size.x.value, size.y.value * 2 + UISpace * 2))
			{
				Tool::HideMoreBar();
			}
		}

		if (!Visible && !pos.y.IsAnimation()) return;

		if (Tool::ToolCount == 0) DrawTool0(window);
		if (Tool::ToolCount == 1) DrawTool1(window);

		if (XMsg::MouseMsg::IsMouseIn(
			pos.x.value - size.x.value * CenterX, pos.y.value - size.y.value * CenterY, size.x.value, size.y.value))
		{
			Tool::IsInBar = true;
		}

		Sprite s(rt.getTexture());
		
		s.setPosition({ pos.x.value - 1 - size.x.value * CenterX,pos.y.value - 1 - size.y.value * CenterY });
		s.setColor(Color(255, 255, 255, UIAlpha));
		window.draw(s);

		if (!PerNeedRedraw) NeedReDraw = false;
		else PerNeedRedraw = false;
	}

	void SetVisible(bool visible)
	{
		Visible = visible;

		if (visible) pos.y.SetAnimation(WindowSize.y - UISpace - BasicSize - UISpace - BasicSize,TotalFrame);
		else pos.y.SetAnimation(WindowSize.y, TotalFrame);

		if (size.y.value <= 0) size.y.SetAnimationStartValue(BasicSize);
		if (pos.x.value != MainToolBar.pos.x.value) pos.x.SetAnimationStartValue(MainToolBar.pos.x.value);

		if(Visible)
		{
			//笔刷，颜色
			if (Tool::ToolCount == 0)
			{
				if (size.x.value != BasicSize * 8 || size.x.end != BasicSize * 8)
				{
					size.x.SetAnimation(BasicSize * 8, TotalFrame);
				}
			}
			//其余
			else
			{
				if (size.x.value != BasicSize * 5 || size.x.end != BasicSize * 5)
				{
					size.x.SetAnimation(BasicSize * 5, TotalFrame);
				}
			}
		}

		NeedReDraw = true;
	}
};
static MoreBar MoreToolBar;

void SetMoreBarVisible(bool visible)
{
	MoreToolBar.SetVisible(visible);

	//笔刷，颜色
	if (Tool::ToolCount == 0)
	{
		if (visible)
		{
			if (MainToolBar.size.x.value < BasicSize * 8)
			{
				MainToolBar.size.x.SetAnimation(BasicSize * 8, TotalFrame);
			}
		}
		else
		{
			if (MainToolBar.size.x.value > BasicSize * 5)
			{
				MainToolBar.size.x.SetAnimation(BasicSize * 5, TotalFrame);
			}
		}
	}
	else
	{
		if (MainToolBar.size.x.value > BasicSize * 5)
		{
			MainToolBar.size.x.SetAnimation(BasicSize * 5, TotalFrame);
		}
	}

	//设置底部消息偏移
	if (visible) BottomMessage::SetYOffset(-BasicSize - UISpace);
	else BottomMessage::SetYOffset(0);
}
static void UpdateMoreBarVisible()
{
	bool isv = !MoreToolBar.Visible;
	SetMoreBarVisible(isv);
}

void Tool::HideMoreBar()
{
	if (!MoreToolBar.Visible) return;
	else SetMoreBarVisible(false);
}

#pragma endregion

//几何图库
#pragma region MyRegion

//提取文件名
wstring GetFileNameWithoutExtension(const std::wstring& path) {
	// 先找最后一个路径分隔符
	size_t lastSlash = path.find_last_of(L"/\\");
	size_t start = (lastSlash == std::wstring::npos) ? 0 : lastSlash + 1;

	// 从文件名部分找最后一个点号
	size_t lastDot = path.find_last_of(L'.');

	// 如果没有点号，或者点号在路径分隔符之前（说明没有后缀），返回整个文件名
	if (lastDot == std::wstring::npos || lastDot < start) {
		return path.substr(start);
	}

	// 返回文件名部分（不含后缀）
	return path.substr(start, lastDot - start);
}

bool NeedEnterGeometricLibrary = false;
void EnterGeometricLibrary(RenWin& window)
{
	NeedEnterGeometricLibrary = false;

	wstring LibPath = filesystem::current_path().wstring() + L"\\GeometricLibrary";

	IMAGE temp;
	XImage::NewImage(window, temp);

	static int BarW = BasicSize * 10, BarH = BasicSize * 6;

	EV BarY;
	BarY.SetAnimationStartValue(WindowSize.y);
	BarY.SetAnimation(WindowSize.y - BarH - BasicSize - UISpace * 2, TotalFrame);
	EV BackAlpha;
	BackAlpha.SetAnimationStartValue(255);
	BackAlpha.SetAnimation(50, TotalFrame);

	static int BarX = WindowSize.x / 2 - BarW / 2;

	struct Lib
	{
		wstring name;
		wstring path;
		IMAGE img;
		EV Scale;
	};
	vector<Lib> libs;
	int index = 0;
	static wstring SubPathName[3] = { L"Basic",L"2D",L"3D" };
	wstring LibPathSub = LibPath + L"\\" + SubPathName[index];

	vector<Path> FilePath = XFile::ListFiles(LibPathSub);
	for (int i = 0; i < FilePath.size(); i++)
	{
		libs.emplace_back();

		libs.back().name = GetFileNameWithoutExtension(FilePath[i].wstring());
		libs.back().path = FilePath[i].wstring();
		XImage::NewImage(libs.back().img, FilePath[i].wstring());
		libs.back().Scale.SetAnimationStartValue(0);
		libs.back().Scale.SetAnimation(100, TotalFrame);
	}

	bool NeedExit = false;

	//菜单底线长度
	EV MenuBottomLineLengh;
	MenuBottomLineLengh.SetAnimation(BarW * 0.05, TotalFrame);

	while (!XMsg::IsClose(window) && !NeedExit)
	{
		XWindow::DelayFps(window,60);

		//更新动画
		BarY.UpdateAnimation(XEase::EaseBasic::easeOut, 6);
		BackAlpha.UpdateAnimation(XEase::EaseBasic::linear);

		//绘制背景
		temp.color.a = BackAlpha.value;
		XImage::PutImage(temp, 0, 0, window);

		//绘制底色
		XGraph::SetFillColor(Color(50, 50, 50, 100));
		XGraph::RectangleShape::FillRoundRect_WithoutBorder(BarX, BarY.value, BarW, BarH, RoundSize, window);

		//绘制侧边栏
		XGraph::SetFillColor(Color(10, 10, 10, 150));
		XGraph::RectangleShape::FillRoundRect_WithoutBorder(BarX, BarY.value, BarW * 0.15, BarH, RoundSize, window);

		//绘制玻璃边框
		DrawGlassBar(BarX, BarY.value, BarW, BarH, RoundSize, window);

		XText::SetFontConfig(Color::White, FONTSIZE);
		XText::SetFontAdjust(ADJUST_CENTER , ADJUST_TOP);
		XText::Xyprintf(BarX + BarW * 0.075, BarY.value + BarH * 0.05, "几何图库", window);

		//菜单底线动画更新
		if (!BarY.IsAnimation()) MenuBottomLineLengh.UpdateAnimation(XEase::EaseBasic::easeOut, 5);

		//绘制菜单
		static wstring menu[3] = { L"基础",L"平面",L"立体" };
		for (int i = 0; i < 3; i++)
		{
			XText::Xyprintf(BarX + BarW * 0.075, BarY.value + BarH * 0.15 + BarH * i * 0.1, menu[i], window);

			if(i == index)
			{
				static int lw = WindowSize.x / 200;
				XGraph::LineShape::SetLineWidth(lw);
				XGraph::LineShape::Line(
					BarX + BarW * 0.075 - MenuBottomLineLengh.value, BarY.value + BarH * 0.21 + BarH * i * 0.1,
					BarX + BarW * 0.075 + MenuBottomLineLengh.value, BarY.value + BarH * 0.21 + BarH * i * 0.1, window);
			}

			//更新选项
			if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
			{
				if (XMsg::MouseMsg::IsMouseIn(BarX, BarY.value + BarH * 0.12 + BarH * i * 0.1, BarX * 0.3, BarH * 0.1))
				{
					index = i;
					libs.clear();

					wstring LibPathSub = LibPath + L"\\" + SubPathName[index];

					vector<Path> FilePath = XFile::ListFiles(LibPathSub);
					for (int i = 0; i < FilePath.size(); i++)
					{
						libs.emplace_back();

						libs.back().name = GetFileNameWithoutExtension(FilePath[i].wstring());
						libs.back().path = FilePath[i].wstring();
						XImage::NewImage(libs.back().img, FilePath[i].wstring());
						libs.back().Scale.SetAnimationStartValue(0);
						libs.back().Scale.SetAnimation(100, TotalFrame);
					}

					MenuBottomLineLengh.SetAnimationStartValue(0);
					MenuBottomLineLengh.SetAnimation(BarW * 0.05, TotalFrame);
				}
			}
		}


		int xPatch = BarW * 0.2; int yPatch = 0;
		static float ImgBasicScale = IconSize / 1024.0;
		//绘制库
		for (int i = 0; i < libs.size(); i++)
		{
			Lib& b = libs[i];
			float Scale = b.Scale.value / 100.0;

			if(!BarY.IsAnimation())
			{
				if (i == 0) b.Scale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
				else if (libs[i - 1].Scale.frame > TotalFrame / 10) b.Scale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
			}

			//绘制图片
			float ImgScale = ImgBasicScale * Scale;
			XImage::PutScaleImage(b.img,BarX + xPatch + IconSize / 2, BarY.value + BarH * 0.1 + yPatch + IconSize / 2, ImgScale, ImgScale, window,0.5,0.5);
			
			//绘制文字
			static int FontSize = FONTSIZE * 0.9;
			int alpha = 255 * Scale;
			if (alpha > 255) alpha = 255;
			XText::SetFontConfig(Color(255, 255, 255, alpha), FontSize);
			XText::SetFontAdjust(ADJUST_CENTER, ADJUST_TOP);
			XText::Xyprintf(BarX + xPatch + IconSize / 2, BarY.value + BarH * 0.1 + yPatch + IconSize * 1.4, b.name, window);

			//导入检测
			if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
			{
				if (XMsg::MouseMsg::IsMouseIn(BarX + xPatch, BarY.value + BarH * 0.1 + yPatch, IconSize, IconSize))
				{
					if (!ImageManager::AddImageDirectly(b.path))
					{
						RightMessage::ShowMessage(L"无法导入图库", L"插件 - 几何图库", RightMessageType_ERROR, true);
					}
					else
					{
						NeedExit = true;
					}
				}
			}

			xPatch += (IconSize * 2 + UISpace);
			if(i > 4)
			{
				if ((i - 4) % 5 == 0 && i != 0)
				{
					xPatch = BarW * 0.2; yPatch += (IconSize * 2 + UISpace * 2);
				}
			}
			else
			{
				if (i == 4)
				{
					xPatch = BarW * 0.2; yPatch += (IconSize * 2 + UISpace * 2);
				}
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

	BarY.SetAnimation(WindowSize.y, TotalFrame);
	BackAlpha.SetAnimation(255, TotalFrame);

	while (!XMsg::IsClose(window))
	{
		XWindow::DelayFps(window,60);

		//更新动画
		BarY.UpdateAnimation(XEase::EaseBasic::easeOut, 6);
		BackAlpha.UpdateAnimation(XEase::EaseBasic::linear);

		//绘制背景
		temp.color.a = BackAlpha.value;
		XImage::PutImage(temp, 0, 0, window);

		//绘制底色
		if(BarY.value < WindowSize.y - BasicSize)
		{
			XGraph::SetFillColor(Color(50, 50, 50, 100));
			XGraph::RectangleShape::FillRoundRect_WithoutBorder(BarX, BarY.value, BarW, BarH, RoundSize, window);
			DrawGlassBar(BarX, BarY.value, BarW, BarH, RoundSize, window);
		}

		if (!BackAlpha.IsAnimation()) break;
	}

	XMsg::ClearMsg();
	XMsg::SetSleepTime(10);
}

#pragma endregion

//页面选择器
#pragma region MyRegion

void EnterChoosePage(RenWin& window)
{
	NeedEnterChoosePage = false;

	return;

	IMAGE temp;
	XImage::NewImage(window, temp);

	static int BarW = BasicSize * 10, BarH = BasicSize * 6;
	static int BarX = WindowSize.x / 2 - BarW / 2;

	EV BarY;
	BarY.SetAnimationStartValue(WindowSize.y);
	BarY.SetAnimation(WindowSize.y - BarH - BasicSize - UISpace * 2, TotalFrame);
	EV BackAlpha;
	BackAlpha.SetAnimationStartValue(255);
	BackAlpha.SetAnimation(50, TotalFrame);

	//缩略图尺寸
	static int ThumbW = BasicSize * 1.6, ThumbH = BasicSize * 1.2;
	static int ThumbSpace = UISpace;

	//每页缩略图纹理
	vector<RenderTexture> thumbs;
	thumbs.resize(Write::TotalPage);
	for (int i = 0; i < Write::TotalPage; i++)
	{
		thumbs[i] = RenderTexture(Vector2u(ThumbW, ThumbH));
		Write::GetPageThumbnail(i, thumbs[i]);
	}

	//竖向列表布局：每行一个缩略图
	int rowH = ThumbH + ThumbSpace + FONTSIZE;

	//内容可视区域
	int viewX = BarX + BarW * 0.05;
	int viewY = BarY.end + BarH * 0.18;
	int viewW = BarW * 0.9;
	int viewH = BarH * 0.78;

	//内容总高度
	int contentH = Write::TotalPage * rowH;
	int maxScroll = max(0, contentH - viewH);

	//滚动偏移
	float scroll = 0;
	//滚动惯性速度
	float scrollVel = 0;
	//是否正在拖动列表
	bool dragging = false;
	float dragStartY = 0, dragStartScroll = 0;
	//拖动判定阈值
	bool dragMoved = false;

	bool NeedExit = false;

	while (!XMsg::IsClose(window) && !NeedExit)
	{
		XWindow::DelayFps(window,60);

		BarY.UpdateAnimation(XEase::EaseBasic::easeOut, 6);
		BackAlpha.UpdateAnimation(XEase::EaseBasic::linear);

		//滚动惯性
		if (!dragging)
		{
			scroll += scrollVel;
			scrollVel *= 0.9;
			if (fabs(scrollVel) < 0.1) scrollVel = 0;
		}
		if (scroll < 0) { scroll = 0; scrollVel = 0; }
		if (scroll > maxScroll) { scroll = (float)maxScroll; scrollVel = 0; }

		//滚轮
		int wheel = XMsg::Composite::GetMouseRectWheel(viewX, viewY, viewW, viewH);
		if (wheel != 0)
		{
			scroll -= wheel / 120.0 * BasicSize * 0.5;
			if (scroll < 0) scroll = 0;
			if (scroll > maxScroll) scroll = (float)maxScroll;
		}

		//拖动列表
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			if (!dragging && XMsg::MouseMsg::IsMouseIn(viewX, viewY, viewW, viewH))
			{
				dragging = true;
				dragMoved = false;
				dragStartY = (float)XMsg::MouseMsg::GetMousePosWindow().y;
				dragStartScroll = scroll;
			}
		}
		if (dragging)
		{
			if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
			{
				float dy = (float)XMsg::MouseMsg::GetMousePosWindow().y - dragStartY;
				if (fabs(dy) > BasicSize * 0.1) dragMoved = true;

				scroll = dragStartScroll - dy;
				if (scroll < 0) scroll = 0;
				if (scroll > maxScroll) scroll = (float)maxScroll;

				//记录速度用于惯性
				scrollVel = -dy * 0.15;
			}
			else
			{
				dragging = false;
			}
		}

		//绘制背景
		temp.color.a = BackAlpha.value;
		XImage::PutImage(temp, 0, 0, window);

		//绘制底色
		XGraph::SetFillColor(Color(50, 50, 50, 100));
		XGraph::RectangleShape::FillRoundRect_WithoutBorder(BarX, BarY.value, BarW, BarH, RoundSize, window);
		DrawGlassBar(BarX, BarY.value, BarW, BarH, RoundSize, window);

		//标题
		XText::SetFontConfig(Color::White, FONTSIZE);
		XText::SetFontAdjust(ADJUST_CENTER, ADJUST_TOP);
		XText::Xyprintf(BarX + BarW / 2, BarY.value + BarH * 0.05, "选择页面", window);

		//裁剪可视区域
		View view(FloatRect(Vector2f((float)viewX, (float)viewY), Vector2f((float)viewW, (float)viewH)));
		window.setView(view);

		//绘制竖向缩略图列表
		for (int i = 0; i < Write::TotalPage; i++)
		{
			int x = viewX;
			int y = viewY + i * rowH - (int)scroll;

			//跳过不可见项
			if (y + rowH < viewY || y > viewY + viewH) continue;

			//当前页高亮
			if (i == Write::Page)
			{
				XGraph::SetColor(User::MainColor);
				XGraph::LineShape::SetLineWidth(WindowSize.x / 400);
				XGraph::RectangleShape::Rect(x - 2, y - 2, ThumbW + 4, ThumbH + 4, window);
			}

			//绘制缩略图
			Sprite s(thumbs[i].getTexture());
			s.setPosition(Vector2f((float)x, (float)y));
			window.draw(s);

			//页码
			XText::SetFontConfig(Color::White, FONTSIZE * 0.7);
			XText::SetFontAdjust(ADJUST_LEFT, ADJUST_CENTER);
			XText::Xyprintf(x + ThumbW + ThumbSpace, y + ThumbH / 2, L"第" + to_wstring(i + 1) + L"页", window);

			//点击跳转（未发生拖动时）
			if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft) && !dragMoved)
			{
				if (XMsg::MouseMsg::IsMouseIn(x, y, ThumbW, ThumbH))
				{
					Write::GoToPage(i);
					UpdatePageBar();
					NeedExit = true;
				}
			}
		}

		//恢复视图
		window.setView(window.getDefaultView());

		//绘制滚动条
		if (maxScroll > 0)
		{
			int barW = WindowSize.x / 300;
			int barX = BarX + BarW - barW * 2;
			int barTrackH = viewH;
			int barH = max(BasicSize * 0.5, barTrackH * viewH / contentH);
			int barY = viewY + (barTrackH - barH) * scroll / maxScroll;

			XGraph::SetFillColor(Color(255, 255, 255, 40));
			XGraph::RectangleShape::FillRoundRect_WithoutBorder(barX, viewY, barW, barTrackH, barW / 2, window);

			XGraph::SetFillColor(Color(255, 255, 255, 120));
			XGraph::RectangleShape::FillRoundRect_WithoutBorder(barX, barY, barW, barH, barW / 2, window);
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


	BarY.SetAnimation(WindowSize.y, TotalFrame);
	BackAlpha.SetAnimation(255, TotalFrame);

	while (!XMsg::IsClose(window))
	{
		XWindow::DelayFps(window,60);

		BarY.UpdateAnimation(XEase::EaseBasic::easeOut, 6);
		BackAlpha.UpdateAnimation(XEase::EaseBasic::linear);

		temp.color.a = BackAlpha.value;
		XImage::PutImage(temp, 0, 0, window);

		if (BarY.value < WindowSize.y - BasicSize)
		{
			XGraph::SetFillColor(Color(50, 50, 50, 100));
			XGraph::RectangleShape::FillRoundRect_WithoutBorder(BarX, BarY.value, BarW, BarH, RoundSize, window);
			DrawGlassBar(BarX, BarY.value, BarW, BarH, RoundSize, window);
		}

		if (!BackAlpha.IsAnimation()) break;
	}

	XMsg::ClearMsg();
	XMsg::SetSleepTime(10);
}


#pragma endregion

//终端
#pragma region MyRegion


bool NeedEnterTerminal = false;
void EnterTerminal(RenWin& window)
{
	NeedEnterTerminal = false;
}


#pragma endregion

//插件
#pragma region MyRegion

class ExtBar
{
	struct ext
	{
		wstring name;
		wstring path;
		IMAGE img;
		EV IconScale;
	};
	vector<ext> exts;

	bool init = false;
	bool NeedReDraw = true;

	EV2 size, pos;

	wstring ExtenPath = filesystem::current_path().wstring() + L"\\Extens\\";

	void StartClock()
	{
		if (XFile::Exists(ExtenPath + L"Clock.exe"))
		{
			ShellExecuteW(NULL, L"open", (ExtenPath + L"Clock.exe").c_str(), NULL, NULL, SW_SHOW);

			BottomMessage::AddMessage(106, L"正在加载：计时器", BottomMessageType_SUCCESS, 180);
		}
		else
		{
			RightMessage::ShowMessage(
				L"无法加载定时器插件，这可能是MiuBarrd的配置出现错误，错误代码：NOLNK", L"加载插件", RightMessageType_ERROR);

			BottomMessage::AddMessage(106, L"加载插件失败", BottomMessageType_ERROR, 180);
		}
	}
	void StartGeometricLibrary()
	{
		NeedEnterGeometricLibrary = true;
	}
	void StartMini()
	{
		if (XFile::Exists(ExtenPath + L"MiniMiuBarrd.exe"))
		{
			ShellExecuteW(NULL, L"open", (ExtenPath + L"MiniMiuBarrd.exe").c_str(), NULL, NULL, SW_SHOW);
		}
		else
		{
			RightMessage::ShowMessage(
				L"无法加载迷你黑板插件，这可能是MiuBarrd的配置出现错误，错误代码：NOLNK", L"加载插件", RightMessageType_ERROR);

			BottomMessage::AddMessage(106, L"加载插件失败", BottomMessageType_ERROR, 180);
		}
	}
	void StartAI()
	{
		RightMessage::ShowMessage(
			L"终端阻止运行此插件，错误代码：TERMINALSTOP", L"加载插件", RightMessageType_ERROR);

		BottomMessage::AddMessage(106, L"加载插件失败", BottomMessageType_ERROR, 180);
	}
	void StartTerminal()
	{
		NeedEnterTerminal = true;
	}
	void StartElc()
	{
		if (XFile::Exists(ExtenPath + L"Elc.exe"))
		{
			ShellExecuteW(NULL, L"open", (ExtenPath + L"Elc.exe").c_str(), NULL, NULL, SW_SHOW);

			BottomMessage::AddMessage(106, L"正在加载：电学组件", BottomMessageType_SUCCESS, 180);
		}
		else
		{
			RightMessage::ShowMessage(
				L"无法加载电学实验插件，这可能是MiuBarrd的配置出现错误，错误代码：NOLNK", L"加载插件", RightMessageType_ERROR);

			BottomMessage::AddMessage(106, L"加载插件失败", BottomMessageType_ERROR, 180);
		}
	}

	void Run(wstring path,wstring name = L"未知")
	{
		if (path == L"_Clock") StartClock();
		else if (path == L"_GeometricLibrary") StartGeometricLibrary();
		else if (path == L"_MinNiMiuBarrd") StartMini();
		else if (path == L"_AI") StartAI();
		else if (path == L"_Terminal") StartTerminal();
		else if (path == L"_Elc") StartElc();
		else
		{
			if (XFile::Exists(path))
			{
				ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOW);

				BottomMessage::AddMessage(106, L"正在加载：视频展台", BottomMessageType_SUCCESS, 180);
			}
			else
			{
				RightMessage::ShowMessage(
					L"无法加载" + name + L"插件，错误代码：NOLNK", L"加载插件", RightMessageType_ERROR);

				BottomMessage::AddMessage(106, L"加载插件失败", BottomMessageType_ERROR, 180);
			}
		}
	}

public:

	bool Visible = false;

	void Draw(RenWin& window)
	{
		//初始化
		if (!init)
		{
			size.x.SetAnimationStartValue(BasicSize * 5);
			size.y.SetAnimationStartValue(BasicSize * 5);
			pos.y.SetAnimationStartValue(WindowSize.y);
			pos.x.SetAnimationStartValue(WindowSize.x / 2 - size.x.value / 2);

			//初始化插件
#pragma region MyRegion

			static wstring PerExtName[6] = {
				L"计时器",L"几何图库",L"迷你黑板",L"语音助手",L"终端",L"电学组件"
			};
			static wstring PerExtImageName[6] = {
				L"Clock.dll",L"GeometricLibrary.dll",L"MinNiMiuBarrd.dll",L"AI.dll",L"Terminal.dll",L"Elc.dll"
			};
			static wstring PerExtPath[6] = {
				L"_Clock",L"_GeometricLibrary",L"_MinNiMiuBarrd",L"_AI",L"_Terminal",L"_Elc"
			};

			for (int i = 0; i < 6; i++)
			{
				exts.emplace_back();

				auto& ex = exts.back();

				ex.name = PerExtName[i];
				XImage::NewImage(ex.img, ImgPath + L"Exten\\" + PerExtImageName[i]);
				ex.path = PerExtPath[i];

				ex.IconScale.SetAnimationStartValue(0);
				ex.IconScale.SetAnimation(100, TotalFrame);
			}

			ifstream ExtenConfig(filesystem::current_path().wstring() + L"\\Extens\\Config.ini");
			if (ExtenConfig.is_open())
			{
				while (1)
				{
					string temp;

					ExtenConfig >> temp;
					if (temp.empty()) break;

					exts.emplace_back();
					exts.back().name = XString::Convert::utf8_to_wstring(temp);

					ExtenConfig >> temp;
					if (temp.empty())
					{
						exts.pop_back(); break;
					}
					exts.back().path = XString::Convert::utf8_to_wstring(temp);

					ExtenConfig >> temp;
					if (temp.empty())
					{
						exts.pop_back(); break;
					}
					if (!XImage::NewImage(exts.back().img, filesystem::current_path().wstring() + L"\\Exten\\" + XString::Convert::utf8_to_wstring(temp)))
					{
						XImage::NewImage(exts.back().img, ImgPath + L"Exten\\Icon.dll");
					}

					exts.back().IconScale.SetAnimationStartValue(0);
					exts.back().IconScale.SetAnimation(100, TotalFrame);
				}
			}
			else
			{
				ExtenConfig.close();

				ofstream ExtenNew(filesystem::current_path().wstring() + L"\\Extens\\Config.ini");
				if (!ExtenNew.is_open())
				{
					Message::ShowMessage("重建插件引索错误", "错误", ICOTYPE_ERROR, { "确定" }, 3, L"MiuBarrd");
				}
			}

#pragma endregion

			while (exts.size() > 30)
			{
				exts.pop_back();
			}

			init = true;
		}

		//更新动画
		pos.y.UpdateAnimation(XEase::EaseBasic::easeOut, 5);

		//大于可视返回，跳过绘制
		if (pos.y.value > WindowSize.y - BasicSize) return;

		//插件实时绘制
		DrawGlassBar(pos.x.value, pos.y.value, size.x.value, size.y.value, RoundSize, window);

		if(Visible)
		{
			XText::SetFontConfig(Color::White, FONTSIZE * 0.9);
			XText::SetFontAdjust(ADJUST_LEFT, ADJUST_TOP);
			XText::Xyprintf(pos.x.value + BasicSize * 0.5, pos.y.value + BasicSize * 0.1, L"插件列表", window);

			int x = pos.x.value + UISpace * 1.5;
			int yPatch = BasicSize * 0.2;
			static float Scale = IconSize * 0.8 / 512.0;
			for (int i = 0; i < exts.size(); i++)
			{
				auto& ex = exts[i];

				float TotalScale = ex.IconScale.value / 100;

				float ImageScale = Scale * TotalScale;

				if (i % 6 == 0 && i != 0)
				{
					x = pos.x.value + UISpace * 1.5;
					yPatch += IconSize + UISpace;
				}

				//动画更新
				if (pos.y.frame > TotalFrame / 6)
				{
					if (i == 0) ex.IconScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
					else if (exts[i - 1].IconScale.frame > TotalFrame / 15) ex.IconScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
				}

				//绘制底色
				XGraph::SetFillColor(Color(60, 60, 60));
				XGraph::RectangleShape::FillRoundRect_WithoutBorder(
					x - IconSize * 0.1 + IconSize * (1 - TotalScale) * 0.5, 
					pos.y.value + UISpace + yPatch + IconSize * (1 - TotalScale) * 0.55,
					IconSize* TotalScale, IconSize * 1.1 * TotalScale, 
					RoundSize * 0.7, window);

				if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
				{
					if (XMsg::MouseMsg::IsMouseIn(x - IconSize * 0.1, pos.y.value + UISpace + yPatch, IconSize * TotalScale, IconSize * 1.1 * TotalScale))
					{
						Run(ex.path);

						ex.IconScale.SetAnimationStartValue(50);
						ex.IconScale.SetAnimation(100, TotalFrame);
					}
				}

				//绘制图标
				XImage::PutScaleImage(ex.img, x + IconSize * 0.4, pos.y.value + UISpace + yPatch + IconSize * 0.4, ImageScale, ImageScale, window,0.5,0.5);

				//绘制名字
				static int FontSize = FONTSIZE * 0.7;
				XText::SetFontAdjust(ADJUST_CENTER, ADJUST_TOP);
				int alpha = 255 * TotalScale;
				if (alpha > 255) alpha = 255;
				XText::SetFontConfig(Color(255, 255, 255, alpha), FontSize);
				XText::Xyprintf(x + IconSize * 0.4, pos.y.value + UISpace * 1.1 + yPatch + IconSize * 0.8, ex.name, window);

				x += (IconSize * 0.8 + UISpace);
			}

			//自动回收
			if (Tool::ToolCount != 4) SetVisible(false);
		}

		//自动收回
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			if (!XMsg::MouseMsg::IsMouseIn(pos.x.value, pos.y.value, size.x.value, size.y.value + BasicSize * 2))
			{
				SetVisible(false);
				MainToolBar.SetTool(0);
				XMsg::ClearMsg();
				XMsg::SetSleepTime(10);
			}
		}

		NeedReDraw = false;
	}
	void SetVisible(bool v)
	{
		if(v) pos.y.SetAnimation(WindowSize.y - UISpace * 2 - BasicSize * 6, TotalFrame);
		else
		{
			pos.y.SetAnimation(WindowSize.y);

			for (int i = 0; i < exts.size(); i++)
			{
				exts[i].IconScale.SetAnimationStartValue(0);
				exts[i].IconScale.SetAnimation(100, TotalFrame / 2);
			}
		}

		Visible = v;
	}
};
static ExtBar ExtToolBar;

void SetExtV(bool v)
{
	if (ExtToolBar.Visible == v) return;

	ExtToolBar.SetVisible(v);
}
void UpdateExt()
{
	ExtToolBar.SetVisible(!ExtToolBar.Visible);
}

#pragma endregion

//开始菜单栏
#pragma region MyRegion

class MinBox
{
	RenderTexture rt;
	bool init = false;

	bool NeedReDraw = true;

	float CenterY = 1;

	int exptime = -1;

	void Min(RenWin& window)
	{
		NeedEnterMin = true;
	}

	//缩放
	EV yScale;

public:

	EV2 pos, size;

	void Draw(RenWin& window)
	{
		static IMAGE Icon;

		//初始化
		if (!init)
		{
			//图标初始化
			wstring ImgName = L"Min.dll";

			if (!LoadIcon(Icon, ImgPath + L"Start\\" + ImgName))
			{
				RightMessage::ShowMessage(L"错误：定位图像Start.Min失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
			}

			size.x.SetAnimationStartValue(BasicSize);
			size.y.SetAnimationStartValue(BasicSize);

			pos.x.SetAnimationStartValue(-BasicSize);
			pos.x.SetAnimation(UISpace * 2 + BasicSize, TotalFrame);

			pos.y.SetAnimationStartValue(WindowSize.y - UISpace);

			//设置缩放动画
			yScale.SetAnimationStartValue(100);

			NeedReDraw = true;

			init = true;
		}

		//脏标记
		if (!NeedReDraw)
			NeedReDraw = IsAnimations(
				size.x,size.y
				);

		//动画更新
		pos.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		pos.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		//绘制
		if (NeedReDraw || UpdateUser > 0)
		{
			if (rt.resize(FTU({ size.x.value + 2,size.y.value + 2 })))
			{
				rt.clear(Color::Transparent);

				//绘制底色
				DrawGlassBar(1, 1, size.x.value, size.y.value, RoundSize, rt);

				static float Scale = IconSize / 512.0;

				//绘制图片
				XImage::PutScaleImage(Icon, size.x.value / 2, size.y.value / 2, Scale, Scale, rt, 0.5, 0.5);

				//绘制文字
				static wstring text = L"最小化";

				static int FontSize = FONTSIZE;
				XText::SetFontConfig(Color::White, FontSize);
				XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
				XText::Xyprintf(size.x.value / 2, size.y.value - BasicSize * 0.1, text, rt);

				rt.display();
			}
		}

		//按键
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			if(XMsg::MouseMsg::IsMouseIn(pos.x.value,pos.y.value - CenterY * size.y.value,size.x.value,size.y.value))
			{
				//最小化
				Min(window);
			}
		}

		if (XMsg::MouseMsg::IsMouseIn(
			pos.x.value, pos.y.value - size.y.value * CenterY, size.x.value, size.y.value))
		{
			Tool::IsInBar = true;
		}

		if (pos.y.frame > MinFrame) yScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		Sprite s(rt.getTexture());
		
		s.setPosition({ pos.x.value - 1,pos.y.value - 1 - CenterY * size.y.value });
		s.setColor(Color(255, 255, 255, UIAlpha));
		s.setScale(Vector2f(1, yScale.value / 100.0));
		window.draw(s);

		//自动回放检测
		if (exptime > -1)
		{
			exptime -= 1;
			if (exptime == 0)
			{
				static int ExpPos = WindowSize.y - UISpace;
				pos.y.SetAnimation(ExpPos, TotalFrame);

				yScale.SetAnimationStartValue(ScaleStrgengh);
				yScale.SetAnimation(100, TotalFrame);
			}
		}

		NeedReDraw = false;
	}
	void SetX(int x,int TotalFrame)
	{
		pos.x.SetAnimation(x, TotalFrame);
	}

	void exp()
	{
		exptime = EXP_TIME;

		static int ExpPos = WindowSize.y - UISpace + BasicSize * 0.9;
		if (pos.y.IsAnimation() || pos.y.value >= ExpPos) return;

		pos.y.SetAnimation(ExpPos, TotalFrame);
	}
};
static MinBox MinToolBar;

class StartBar
{
	EV2 pos, size;

	RenderTexture rt;
	bool init = false;

	bool NeedReDraw = true;

	float CenterY = 1;

	int exptime = -1;

	//缩放
	EV yScale;

	void Exit(RenWin& window)
	{
		PostMessage(window.getNativeHandle(), WM_CLOSE, 0, 0);
	}
	void Get(RenWin& window)
	{
		NeedEnterGet = true;
	}
	void Setting(RenWin& window)
	{
		NeedEnterSetting = true;
	}

	void Save(RenWin& window)
	{
		NeedEnterSave = true;
	}

public:

	bool Exten = false;

	void Draw(RenWin& window)
	{
		//线条定义
		static EV LinePointY1, LinePointY2,LineLengh1Scale;
		//图标定义
		static IMAGE Icon[4];
		//图标缩放
		static EV IconScale[4];
		//图标位置
		static EV IconX[4];
		//图标文字透明度
		static EV IconTextAlpha[4];
		//“控制中心”透明度
		static EV TextAlpha;

		//初始化
		if (!init)
		{
			//图标初始化
			wstring ImgName[4] = { L"Exit.dll",L"Save.dll",L"Get.dll",L"Setting.dll"};
			for (int i = 0; i < 4; i++)
			{
				if (!LoadIcon(Icon[i], ImgPath + L"Start\\" + ImgName[i]))
				{
					RightMessage::ShowMessage(L"错误：定位图像Start失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
				}

				IconScale[i].SetAnimationStartValue(100);
			}

			size.x.SetAnimationStartValue(BasicSize);
			size.y.SetAnimationStartValue(BasicSize);

			pos.x.SetAnimationStartValue(-BasicSize);
			pos.x.SetAnimation(UISpace, TotalFrame);

			pos.y.SetAnimationStartValue(WindowSize.y - UISpace);

			//设置缩放动画
			yScale.SetAnimationStartValue(100);

			NeedReDraw = true;

			init = true;

			LinePointY1.SetAnimationStartValue(BasicSize * 0.35);
			LinePointY2.SetAnimationStartValue(BasicSize * 0.65);
			LineLengh1Scale.SetAnimationStartValue(100);

			TextAlpha.SetAnimationStartValue(0);

			for (int i = 0; i < 4; i++)
			{
				IconTextAlpha[i].SetAnimationStartValue(0);
				IconX[i].SetAnimationStartValue(-BasicSize * 3);
			}
		}
		
		//动画更新
		pos.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		//使用延迟回收制造流水感
		if(MinToolBar.pos.y.frame > TotalFrame / 10) pos.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		if(size.x.end > size.x.start)
		{
			size.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		}
		else
		{
			size.x.UpdateAnimation(XEase::EaseBasic::easeOut, 4);
		}
		if(size.y.end > size.y.start)
		{
			size.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		}
		else
		{
			size.y.UpdateAnimation(XEase::EaseBasic::easeOut, 4);
		}
		//线段动画更新
		LinePointY1.UpdateAnimation(XEase::EaseBasic::easeOut, 3);
		LinePointY2.UpdateAnimation(XEase::EaseBasic::easeOut, 3);
		LineLengh1Scale.UpdateAnimation(XEase::EaseBasic::easeOut, 3);
		//选项动画更新
		if(TextAlpha.frame > TotalFrame / 10)
		{
			for (int i = 0; i < 4; i++)
			{
				if (i == 0)
				{
					IconX[i].UpdateAnimation(XEase::EaseBasic::easeOut, 4);
					IconScale[i].UpdateAnimation(XEase::EaseBasic::easeInOutBack, 2);
					IconTextAlpha[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
				}
				else if (IconX[i - 1].frame > TotalFrame / 10)
				{
					IconX[i].UpdateAnimation(XEase::EaseBasic::easeOut, 4);
					IconScale[i].UpdateAnimation(XEase::EaseBasic::easeInOutBack, 2);
					IconTextAlpha[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
				}
			}
		}
		//“控制中心”透明度更新
		TextAlpha.UpdateAnimation(XEase::EaseBasic::easeOut, 3);

		//脏标记
		if(!NeedReDraw)
		NeedReDraw = IsAnimations(
			size.x,size.y,LinePointY1,LinePointY2,LineLengh1Scale,IconX[3],TextAlpha,IconTextAlpha[3]
			);

		//绘制
		if (NeedReDraw || UpdateUser > 0)
		{
			if (rt.resize(FTU({ size.x.value + 2,size.y.value + 2 })))
			{
				rt.clear(Color::Transparent);

				//绘制底色
				DrawGlassBar(1, 1, size.x.value, size.y.value, RoundSize, rt);

				//绘制线条
				static int lw = WindowSize.x / 250;
				XGraph::SetColor(Color::White);
				XGraph::LineShape::SetLineWidth(lw);
				static int x1 = BasicSize * 0.25, x2 = BasicSize * 0.75;
				static int y1 = BasicSize * 0.35, y2 = BasicSize * 0.5, y3 = BasicSize * 0.65;
				XGraph::LineShape::Line(x1, LinePointY1.value + size.y.value - BasicSize, x2, y1 + size.y.value - BasicSize, rt);
				XGraph::LineShape::Line(x1, LinePointY2.value + size.y.value - BasicSize, x2, y3 + size.y.value - BasicSize, rt);
				int LineScaleSize = (x2 - x1) * LineLengh1Scale.value / 100.0;
				XGraph::LineShape::Line(x1,y2 + size.y.value - BasicSize, x1 + LineScaleSize, y2 + size.y.value - BasicSize, rt);

				//绘制“控制中心”
				if (TextAlpha.value > 0)
				{
					XText::SetFontConfig(Color(255, 255, 255, TextAlpha.value), FONTSIZE * 1.2);
					XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
					XText::Xyprintf(size.x.value / 1.9, size.y.value - BasicSize * 0.5, L"控制中心",rt);
				}

				//绘制选项
				for (int i = 0; i < 4; i++)
				{
					//绘制底色
					XGraph::SetFillColor(Color(255, 255, 255, 20));
					XGraph::RectangleShape::FillRoundRect_WithoutBorder(
						1 + IconX[i].value + size.x.value * 0.05, 1 + size.y.value - BasicSize * (i + 2) * 0.9 + BasicSize * 0.05, size.x.value * 0.9, BasicSize * 0.8, RoundSize / 2, rt);

					//当处于可见范围时才绘制
					if (IconX[i].value > -BasicSize * 3)
					{
						//绘制图标
						int x = IconX[i].value + BasicSize * 0.5, y = 1 + size.y.value - BasicSize * (i + 2) * 0.9 + BasicSize * 0.5;
						static float ImgScale = IconSize * 0.8 / 512.0;
						XImage::PutScaleImage(Icon[i], x, y, ImgScale* IconScale[i].value / 100.0, ImgScale* IconScale[i].value / 100.0,rt, 0.5, 0.5);

						//绘制文字
						static wstring name[4] = { L"退出",L"保存板书",L"打开图片/文件",L"设置"};
						XText::SetFontConfig(Color(255,255,255, IconTextAlpha[i].value), FONTSIZE * 1.2);
						XText::SetFontAdjust(ADJUST_LEFT, ADJUST_CENTER);
						XText::Xyprintf(IconX[i].value + BasicSize * 1.2, y, name[i], rt);
					}

				}

				rt.display();
			}
		}

		//按键
		if(XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			//展开收缩检测
			if (XMsg::MouseMsg::IsMouseIn(pos.x.value + 1, pos.y.value + 1 - CenterY * size.y.value + size.y.value - BasicSize, BasicSize, BasicSize))
			{
				Exten = !Exten;

				if (Exten)
				{
					//设置大小动画
					size.x.SetAnimation(BasicSize * 3, TotalFrame);
					size.y.SetAnimation(BasicSize * 4.6, TotalFrame);
					//设置线段动画
					LinePointY1.SetAnimation(BasicSize * 0.5, TotalFrame / 2);
					LinePointY2.SetAnimation(BasicSize * 0.5, TotalFrame / 2);
					LineLengh1Scale.SetAnimation(0, TotalFrame / 2);

					//设置图标动画
					for (int i = 0; i < 4; i++)
					{
						//设置图标缩放
						IconScale[i].SetAnimationStartValue(30);
						IconScale[i].SetAnimation(100, TotalFrame);
						//设置图标位置偏移
						IconX[i].SetAnimation(1, TotalFrame);
						//设置图标文字透明度
						IconTextAlpha[i].SetAnimation(255, TotalFrame);
					}

					//设置“控制中心”透明度
					TextAlpha.SetAnimation(255, TotalFrame);

					//设置最小化位置
					MinToolBar.SetX(BasicSize * 3 + UISpace * 2, TotalFrame * 1.2);
				}
				else
				{
					//设置大小动画
					size.x.SetAnimation(BasicSize, TotalFrame / 1.5);
					size.y.SetAnimation(BasicSize, TotalFrame / 1.5);
					//设置线段动画
					LinePointY1.SetAnimation(BasicSize * 0.35, TotalFrame / 2);
					LinePointY2.SetAnimation(BasicSize * 0.65, TotalFrame / 2);
					LineLengh1Scale.SetAnimation(100, TotalFrame / 2);

					for (int i = 0; i < 4; i++)
					{
						//设置图标位置偏移
						IconX[i].SetAnimation(-BasicSize * 3, TotalFrame);
						//设置图标文本透明度
						IconTextAlpha[i].SetAnimation(0, TotalFrame);
					}
					//设置“控制中心”透明度
					TextAlpha.SetAnimation(0, TotalFrame);

					//设置最小化位置
					MinToolBar.SetX(BasicSize + UISpace * 2, TotalFrame * 1.2);
				}
			}

			if (XMsg::MouseMsg::IsMouseIn(pos.x.value + 1, pos.y.value + 1 - CenterY * size.y.value, size.x.value, size.y.value))
			{
				//展开按键检测
				if (Exten)
				{
					for (int i = 0; i < 4; i++)
					{
						if (XMsg::MouseMsg::IsMouseIn(
							pos.x.value + 1 + IconX[i].value + size.x.value * 0.05,
							pos.y.value + 1 + size.y.value - size.y.value * CenterY - BasicSize * (i + 2) * 0.9 + BasicSize * 0.05, 
							size.x.value * 0.9, BasicSize * 0.8))
						{
							if (i == 0) //退出
							{
								Exit(window);
							}
							if (i == 2) //打开文件导入
							{
								Get(window);
							}
							if (i == 1) //打开保存/打开
							{
								Save(window);
							}
							if (i == 3) //打开设置
							{
								Setting(window);
							}
						}
					}
				}
			}
			else //点击外部，自动收回菜单
			{
				//设置大小动画
				size.x.SetAnimation(BasicSize, TotalFrame / 1.5);
				size.y.SetAnimation(BasicSize, TotalFrame / 1.5);
				//设置线段动画
				LinePointY1.SetAnimation(BasicSize * 0.35, TotalFrame / 2);
				LinePointY2.SetAnimation(BasicSize * 0.65, TotalFrame / 2);
				LineLengh1Scale.SetAnimation(100, TotalFrame / 2);

				for (int i = 0; i < 3; i++)
				{
					//设置图标位置偏移
					IconX[i].SetAnimation(-BasicSize * 3, TotalFrame);
					//设置图标文本透明度
					IconTextAlpha[i].SetAnimation(0, TotalFrame);
				}
				//设置“控制中心”透明度
				TextAlpha.SetAnimation(0, TotalFrame);

				//设置最小化位置
				MinToolBar.SetX(BasicSize + UISpace * 2, TotalFrame * 1.2);
			}
		}

		if (XMsg::MouseMsg::IsMouseIn(
			pos.x.value, pos.y.value - size.y.value * CenterY, size.x.value, size.y.value))
		{
			Tool::IsInBar = true;
		}

		if (pos.y.frame > MinFrame) yScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		Sprite s(rt.getTexture());
		
		s.setPosition({ pos.x.value - 1,pos.y.value - 1 - CenterY  * size.y.value});
		s.setColor(Color(255, 255, 255, UIAlpha));
		s.setScale(Vector2f(1, yScale.value / 100.0));
		window.draw(s);

		//自动回放检测
		if (exptime > -1)
		{
			exptime -= 1;
			if (exptime == 0)
			{
				static int ExpPos = WindowSize.y - UISpace;
				pos.y.SetAnimation(ExpPos, TotalFrame);

				yScale.SetAnimationStartValue(ScaleStrgengh);
				yScale.SetAnimation(100, TotalFrame);
			}
		}

		NeedReDraw = false;
	}

	void exp()
	{
		exptime = EXP_TIME;

		static int ExpPos = WindowSize.y - UISpace + BasicSize * 0.9;
		if (pos.y.IsAnimation() || pos.y.value >= ExpPos) return;

		pos.y.SetAnimation(ExpPos, TotalFrame);
	}
};
static StartBar StartToolBar;

#pragma endregion

//页面管理工具栏
#pragma region MyRegion

class PageBar
{
	RenderTexture rt;
	bool init = false;

	bool PerNeedReDraw = false;

	void LastPage()
	{
		Write::LastPage();
		NeedReDraw = PerNeedReDraw = true;
	}
	void ChoosePage()
	{
		NeedEnterChoosePage = true;
	}

	void NextPage()
	{
		Write::NextPage();
		NeedReDraw = PerNeedReDraw = true;
	}

	//收缩时间
	int exptime = -1;

	//缩放
	EV yScale;

public:
	EV2 pos, size;

	bool NeedReDraw = true;

	float CenterX = 0, CenterY = 0;

	void Draw(RenWin& window)
	{
		//文字透明度
		static EV TextAlpha;
		//箭头偏移度
		static EV ArrowOffsetLeft, ArrowOffsetRight;
		//箭头透明度
		static EV ArrowAlpha;
		//底部文字透明度
		static EV BottomTextAlpha[3];
		//底部文字偏移
		static EV BottomTextOffset[3];

		if (!init)
		{
			pos.x.SetAnimationStartValue(WindowSize.x);
			pos.x.SetAnimation(WindowSize.x - UISpace - BasicSize * 3,TotalFrame);
			pos.y.SetAnimationStartValue(WindowSize.y - UISpace - BasicSize);

			size.x.SetAnimationStartValue(BasicSize * 3);
			size.y.SetAnimationStartValue(BasicSize);

			//设置缩放动画
			yScale.SetAnimationStartValue(100);

			TextAlpha.SetAnimation(255, TotalFrame);

			ArrowOffsetLeft.SetAnimationStartValue(BasicSize);
			ArrowOffsetLeft.SetAnimation(0, TotalFrame);
			ArrowOffsetRight.SetAnimationStartValue(BasicSize);
			ArrowOffsetRight.SetAnimation(0, TotalFrame);
			ArrowAlpha.SetAnimation(255, TotalFrame);

			for (int i = 0; i < 3; i++)
			{
				BottomTextAlpha[i].SetAnimation(255, TotalFrame);
				BottomTextOffset[i].SetAnimationStartValue(BasicSize * 0.2);
				BottomTextOffset[i].SetAnimation(0, TotalFrame / 2);
			}

			NeedReDraw = true;

			init = true;
		}

		if (!NeedReDraw)
			NeedReDraw = IsAnimations(
				TextAlpha,ArrowOffsetLeft,ArrowOffsetRight,ArrowAlpha,BottomTextAlpha[2]
				);

		//动画更新
		pos.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1);
		pos.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1);

		//更新文字动画
		TextAlpha.UpdateAnimation(XEase::EaseBasic::easeOut, 3);
		if (!TextAlpha.IsAnimation())
		{
			//更新箭头动画
			ArrowOffsetLeft.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1);
			ArrowOffsetRight.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1);
			ArrowAlpha.UpdateAnimation(XEase::EaseBasic::easeOut, 3);

			if (!ArrowAlpha.IsAnimation())
			{
				//更新底部文字动画
				for (int i = 0; i < 3; i++)
				{
					if (i == 0)
					{
						BottomTextOffset[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
						BottomTextAlpha[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
					}
					else if(BottomTextAlpha[i -1].frame> TotalFrame / 10)
					{
						BottomTextOffset[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
						BottomTextAlpha[i].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
					}
				}
			}
		}

		int page = Write::Page + 1;
		int totalpage = Write::TotalPage;

		//绘制
		if (NeedReDraw || UpdateUser > 0)
		{
			if (rt.resize(FTU({ size.x.value + 2,size.y.value + 2 })))
			{
				rt.clear(Color::Transparent);

				//绘制底色
				DrawGlassBar(1, 1, size.x.value, size.y.value, RoundSize, rt);

				if(!WriteCamera::EnableWriteCamera)
				{
					//绘制页面
					if (TextAlpha.value > 0)
					{
						static int FontSize = FONTSIZE * 1.5;

						XText::SetFontConfig(Color(255, 255, 255, TextAlpha.value), FontSize);
						XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
						static int x = BasicSize * 1.5, y = BasicSize * 0.5;

						wstring text = to_wstring(page) + L"/" + to_wstring(totalpage);
						XText::Xyprintf(x, y, text, rt);

						static int lw = WindowSize.x / 300;
						XGraph::LineShape::SetLineWidth(lw);

						static int x1 = BasicSize * 0.4, x2 = BasicSize * 0.6;
						static int y1 = BasicSize * 0.5, y2 = BasicSize * 0.35, y3 = BasicSize * 0.65;

						//左箭头
						if (page > 1) XGraph::SetColor(Color(255, 255, 255, ArrowAlpha.value));
						else XGraph::SetColor(Color(150, 150, 150, ArrowAlpha.value));
						XGraph::LineShape::Line(x1 + ArrowOffsetLeft.value, y1, x2 + ArrowOffsetLeft.value, y2, rt);
						XGraph::LineShape::Line(x1 + ArrowOffsetLeft.value, y1, x2 + ArrowOffsetLeft.value, y3, rt);

						XGraph::SetColor(Color(255, 255, 255, ArrowAlpha.value));
						if (page < totalpage)
						{
							static int x3 = BasicSize * 2.6, x4 = BasicSize * 2.4;

							//右箭头
							XGraph::LineShape::Line(x3 - ArrowOffsetRight.value, y1, x4 - ArrowOffsetRight.value, y2, rt);
							XGraph::LineShape::Line(x3 - ArrowOffsetRight.value, y1, x4 - ArrowOffsetRight.value, y3, rt);
						}
						else
						{
							static int x3 = BasicSize * 2.65, x4 = BasicSize * 2.35, x5 = BasicSize * 2.5;

							//右加号
							XGraph::LineShape::Line(x3 - ArrowOffsetRight.value, y1, x4 - ArrowOffsetRight.value, y1, rt);
							XGraph::LineShape::Line(x5 - ArrowOffsetRight.value, y2, x5 - ArrowOffsetRight.value, y3, rt);
						}
					}

					//绘制底部文字
					for (int i = 0; i < 3; i++)
					{
						static wstring text[3] = { L"上一页",L"页数",L"下一页" };

						if (BottomTextAlpha[i].value > 0)
						{
							static int FontSize = FONTSIZE;

							XText::SetFontConfig(Color(255, 255, 255, BottomTextAlpha[i].value), FontSize);
							XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);

							XText::Xyprintf(BasicSize * i + BasicSize * 0.5, BasicSize * 0.9 + BottomTextOffset[i].value, text[i], rt);
						}
					}
				}
				else
				{
					//绘制页面
					if (TextAlpha.value > 0)
					{
						static int FontSize = FONTSIZE * 1.5;

						XText::SetFontConfig(Color(255, 255, 255, TextAlpha.value), FontSize);
						XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
						static int x = BasicSize * 1.5, y = BasicSize * 0.5;

						wstring text = to_wstring(page - 1) + L"/相册/" + to_wstring(totalpage - 1);
						if (page == 1) text = L"展台/" + to_wstring(totalpage - 1);
						XText::Xyprintf(x, y, text, rt);

						
						static int lw = WindowSize.x / 300;
						XGraph::LineShape::SetLineWidth(lw);

						static int x1 = BasicSize * 0.4, x2 = BasicSize * 0.6;
						static int y1 = BasicSize * 0.5, y2 = BasicSize * 0.35, y3 = BasicSize * 0.65;

						if (page > 1)
						{
							//左箭头
							if (page > 2) XGraph::SetColor(Color(255, 255, 255, ArrowAlpha.value));
							else XGraph::SetColor(Color(150, 150, 150, ArrowAlpha.value));
							XGraph::LineShape::Line(x1 + ArrowOffsetLeft.value, y1, x2 + ArrowOffsetLeft.value, y2, rt);
							XGraph::LineShape::Line(x1 + ArrowOffsetLeft.value, y1, x2 + ArrowOffsetLeft.value, y3, rt);
						}

						XGraph::SetColor(Color(255, 255, 255, ArrowAlpha.value));
						static int x3 = BasicSize * 2.6, x4 = BasicSize * 2.4;

						//右箭头
						XGraph::SetColor(Color(255, 255, 255, ArrowAlpha.value));
						XGraph::LineShape::Line(x3 - ArrowOffsetRight.value, y1, x4 - ArrowOffsetRight.value, y2, rt);
						XGraph::LineShape::Line(x3 - ArrowOffsetRight.value, y1, x4 - ArrowOffsetRight.value, y3, rt);
						
					}

					//绘制底部文字
					for (int i = 0; i < 3; i++)
					{
						static wstring text[3] = { L"上一张",L"相册",L"下一张" };

						if (page == 1)text[2] = L"进入相册";

						if (WriteCamera::EnableWriteCamera && page == 1 && i < 2) continue;
					
						if (BottomTextAlpha[i].value > 0)
						{
							static int FontSize = FONTSIZE;

							XText::SetFontConfig(Color(255, 255, 255, BottomTextAlpha[i].value), FontSize);
							XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);

							XText::Xyprintf(BasicSize * i + BasicSize * 0.5, BasicSize * 0.9 + BottomTextOffset[i].value, text[i], rt);
						}
					}
				}

				rt.display();
			}
		}

		//按键
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			for (int i = 0; i < 3; i++)
			{
				if (XMsg::MouseMsg::IsMouseIn(pos.x.value + BasicSize * i, pos.y.value, BasicSize, BasicSize))
				{
					if(!WriteCamera::EnableWriteCamera || page > 2)
					{
						if (i == 0)
						{
							LastPage(); //上一页
							ArrowOffsetLeft.SetAnimationStartValue(-BasicSize * 0.1);
							ArrowOffsetLeft.SetAnimation(0, TotalFrame);
						}
						if (i == 1) ChoosePage(); //页面选择
					}
					if (i == 2)
					{
						NextPage(); //下一页/加页
						if(page < totalpage)
						{
							ArrowOffsetRight.SetAnimationStartValue(-BasicSize * 0.1);
							ArrowOffsetRight.SetAnimation(0, TotalFrame);
						}
					}
				}
			}
		}

		if (XMsg::MouseMsg::IsMouseIn(
			pos.x.value - size.x.value * CenterX, pos.y.value - size.y.value * CenterY, size.x.value, size.y.value))
		{
			Tool::IsInBar = true;
		}

		if (pos.y.frame > MinFrame) yScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		Sprite s(rt.getTexture());
		
		s.setPosition({ pos.x.value - 1,pos.y.value - 1 - CenterY * size.y.value });
		s.setColor(Color(255, 255, 255, UIAlpha));
		s.setScale(Vector2f(1, yScale.value / 100.0));
		window.draw(s);

		//自动回放检测
		if (exptime > -1)
		{
			exptime -= 1;
			if (exptime == 0)
			{
				static int ExpPos = WindowSize.y - BasicSize - UISpace;
				pos.y.SetAnimation(ExpPos, TotalFrame);

				yScale.SetAnimationStartValue(ScaleStrgengh);
				yScale.SetAnimation(100, TotalFrame);
			}
		}

		if (!PerNeedReDraw) NeedReDraw = false;
		else PerNeedReDraw = false;
	}

	void exp()
	{
		exptime = EXP_TIME;

		static int ExpPos = WindowSize.y - UISpace - BasicSize * 0.1;
		if (pos.y.IsAnimation() || pos.y.value >= ExpPos) return;

		pos.y.SetAnimation(ExpPos, TotalFrame);
	}
};
static PageBar PageToolBar;

static void UpdatePageBar()
{
	PageToolBar.NeedReDraw = true;
}

#pragma endregion

//滑动工具栏
#pragma region MyRegion

class ScrollBar
{
	RenderTexture rt;
	bool init = false;

	bool NeedReDraw = true;

	EV2 pos[2], size;

	//箭头透明度
	EV ArrowAlpha[2];
	//箭头偏移度
	EV ArrowOffset[2];

	//回收时间
	int exptime = -1;

	void UpPage()
	{
		Write::UpScrollPage();
	}
	void DownPage()
	{
		Write::DownScrollPage();
	}

	bool IsHide = false;

public:

	//左部标签
	bool left = true;

	void Draw(RenWin& window)
	{
		static int BasicSizeTemp = BasicSize * 0.65;

		//初始化
		if (!init)
		{
			if (left)
			{
				pos[0].x.SetAnimationStartValue(-BasicSizeTemp);
				pos[1].x.SetAnimationStartValue(-BasicSizeTemp);
			}
			else
			{
				pos[0].x.SetAnimationStartValue(BasicSizeTemp);
				pos[1].x.SetAnimationStartValue(BasicSizeTemp);
				
			}

			pos[0].x.SetAnimation(2, TotalFrame);
			pos[1].x.SetAnimation(2, TotalFrame);

			pos[0].y.SetAnimationStartValue(2);
			pos[1].y.SetAnimationStartValue(BasicSizeTemp + UISpace);

			size.x.SetAnimationStartValue(BasicSizeTemp);
			size.y.SetAnimationStartValue(BasicSizeTemp);

			if (!rt.resize(ITU({ BasicSizeTemp + 4,BasicSizeTemp * 2 + UISpace + 4 })))
			{
				Message::ShowMessage("处理ScrollBar纹理错误", "错误", ICOTYPE_ERROR, { "确定" }, 3, L"MiuBarrd");
				XWindow::SetBackGroundColor(Color(30, 30, 30));
			}

			ArrowAlpha[0].SetAnimation(255, TotalFrame);
			ArrowAlpha[1].SetAnimation(255, TotalFrame);

			ArrowOffset[0].SetAnimationStartValue(-BasicSizeTemp * 0.2);
			ArrowOffset[0].SetAnimation(0, TotalFrame);
			ArrowOffset[1].SetAnimationStartValue(-BasicSizeTemp * 0.2);
			ArrowOffset[1].SetAnimation(0, TotalFrame);

			init = true;
		}

		//动画更新
		pos[0].x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		if (!pos[0].x.IsAnimation())
		{
			ArrowAlpha[0].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
			ArrowOffset[0].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
		}
		if(pos[0].x.frame > TotalFrame / 10)
		{
			pos[1].x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
			
			if (!pos[1].x.IsAnimation())
			{
				ArrowAlpha[1].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
				ArrowOffset[1].UpdateAnimation(XEase::EaseBasic::easeOut, 3);
			}
		}

		//脏标记
		if (!NeedReDraw)
			NeedReDraw = IsAnimations(
			pos[0].x,pos[1].x,ArrowAlpha[0],ArrowAlpha[1],ArrowOffset[0],ArrowOffset[1]
				);

		if (IsHide && !NeedReDraw) return;

		//绘制
		if (NeedReDraw || UpdateUser > 0)
		{
			rt.clear(Color::Transparent);

			//底色
			static int RoundSizeTemp = RoundSize * 0.8;
			DrawGlassBar(pos[0].x.value, pos[0].y.value, BasicSizeTemp, BasicSizeTemp, RoundSizeTemp, rt);
			DrawGlassBar(pos[1].x.value, pos[1].y.value, BasicSizeTemp, BasicSizeTemp, RoundSizeTemp, rt);

			//箭头
			static int lw = WindowSize.x / 300;
			XGraph::LineShape::SetLineWidth(lw);

			if (ArrowAlpha[0].value > 0)
			{
				XGraph::SetColor(Color(255, 255, 255, ArrowAlpha[0].value));
				int x1 = pos[0].x.value + BasicSizeTemp * 0.35, x2 = pos[0].x.value + BasicSizeTemp * 0.5, x3 = pos[0].x.value + BasicSizeTemp * 0.65;
				static int y1 = 2 + BasicSizeTemp * 0.4, y2 = 2 + BasicSizeTemp * 0.6;

				XGraph::LineShape::Line(x1, y2 - ArrowOffset[0].value, x2, y1 - ArrowOffset[0].value, rt);
				XGraph::LineShape::Line(x3, y2 - ArrowOffset[0].value, x2, y1 - ArrowOffset[0].value, rt);
			}
			if (ArrowAlpha[1].value > 0)
			{
				XGraph::SetColor(Color(255, 255, 255, ArrowAlpha[1].value));
				int x1 = pos[1].x.value + BasicSizeTemp * 0.35, x2 = pos[1].x.value + BasicSizeTemp * 0.5, x3 = pos[1].x.value + BasicSizeTemp * 0.65;
				static int y2 = 2 + BasicSizeTemp * 1.4 + UISpace, y1 = 2 + BasicSizeTemp * 1.6 + UISpace;

				XGraph::LineShape::Line(x1, y2 + ArrowOffset[1].value, x2, y1 + ArrowOffset[1].value, rt);
				XGraph::LineShape::Line(x3, y2 + ArrowOffset[1].value, x2, y1 + ArrowOffset[1].value, rt);
			}
			

			rt.display();
		}

		//按键
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			if(left)
			{
				static float x = UISpace - 2;
				static float y = WindowSize.y / 2 - BasicSizeTemp - UISpace;

				if (XMsg::MouseMsg::IsMouseIn(x + pos[0].x.value, y + pos[0].y.value, BasicSizeTemp, BasicSizeTemp))
				{
					ArrowOffset[0].SetAnimationStartValue(BasicSizeTemp * 0.2);
					ArrowOffset[0].SetAnimation(0, TotalFrame);
					UpPage();
				}
				if (XMsg::MouseMsg::IsMouseIn(x + pos[1].x.value, y + pos[1].y.value, BasicSizeTemp, BasicSizeTemp))
				{
					ArrowOffset[1].SetAnimationStartValue(BasicSizeTemp * 0.2);
					ArrowOffset[1].SetAnimation(0, TotalFrame);
					DownPage();
				}
			}
			else
			{
				static float x = WindowSize.x - UISpace - BasicSizeTemp - 2;
				static float y = WindowSize.y / 2 - BasicSizeTemp - UISpace;

				if (XMsg::MouseMsg::IsMouseIn(x + pos[0].x.value, y + pos[0].y.value, BasicSizeTemp, BasicSizeTemp))
				{
					ArrowOffset[0].SetAnimationStartValue(BasicSizeTemp * 0.2);
					ArrowOffset[0].SetAnimation(0, TotalFrame);
					UpPage();
				}
				if (XMsg::MouseMsg::IsMouseIn(x + pos[1].x.value, y + pos[1].y.value, BasicSizeTemp, BasicSizeTemp))
				{
					ArrowOffset[1].SetAnimationStartValue(BasicSizeTemp * 0.2);
					ArrowOffset[1].SetAnimation(0, TotalFrame);
					DownPage();
				}
			}
		}

		if(left)
		{
			if (XMsg::MouseMsg::IsMouseIn(
				pos[0].x.value, pos[0].y.value, BasicSizeTemp + 4, BasicSizeTemp * 2 + UISpace + 4))
			{
				Tool::IsInBar = true;
			}
		}
		
		Sprite s(rt.getTexture());
		if(left)
		{
			static float x = UISpace - 2;
			static float y = WindowSize.y / 2 - BasicSizeTemp - UISpace;
			s.setPosition({ x,y });
		}
		else
		{
			static float x = WindowSize.x - UISpace - BasicSizeTemp - 2;
			static float y = WindowSize.y / 2 - BasicSizeTemp - UISpace;
			s.setPosition({ x,y });
		}
		s.setColor(Color(255, 255, 255, UIAlpha));
		window.draw(s);

		//自动回放检测
		if (exptime > -1)
		{
			exptime -= 1;
			if (exptime == 0)
			{
				static int ExpPos = 2;
				pos[0].x.SetAnimation(ExpPos, TotalFrame);
				pos[1].x.SetAnimation(ExpPos, TotalFrame);
			}
		}

		NeedReDraw = false;
	}

	void exp()
	{
		exptime = EXP_TIME;

		if (left)
		{
			static int ExpPos = - BasicSize * 0.5;

			if (!pos[0].x.IsAnimation() && pos[0].x.value > ExpPos)
			{
				pos[0].x.SetAnimation(ExpPos, TotalFrame);
			}
			if (!pos[1].x.IsAnimation() && pos[1].x.value > ExpPos)
			{
				pos[1].x.SetAnimation(ExpPos, TotalFrame);
			}
		}
		else
		{
			static int ExpPos = BasicSize * 0.5;

			if (!pos[0].x.IsAnimation() && pos[0].x.value < ExpPos)
			{
				pos[0].x.SetAnimation(ExpPos, TotalFrame);
			}
			if (!pos[1].x.IsAnimation() && pos[1].x.value < ExpPos)
			{
				pos[1].x.SetAnimation(ExpPos, TotalFrame);
			}
		}
		
	}

	void Hide()
	{
		IsHide = true;
		if(left)
		{
			pos[0].x.SetAnimation(-BasicSize, TotalFrame);
			pos[1].x.SetAnimation(-BasicSize, TotalFrame);
		}
		else
		{
			pos[0].x.SetAnimation(BasicSize, TotalFrame);
			pos[1].x.SetAnimation(BasicSize, TotalFrame);
		}
	}

	void Show()
	{
		IsHide = false;

		pos[0].x.SetAnimation(2, TotalFrame);
		pos[1].x.SetAnimation(2, TotalFrame);
	}
};
static ScrollBar ScrollToolBar[2];

#pragma endregion

//拍照预览
#pragma region MyRegion

bool IsInCamera();

class CameraPreview
{
	RenderTexture rt;
	bool init = false;

	//缩略图重绘脏标记
	bool NeedReDraw = true;

	//每一张缩略图的纹理缓存与缩放动画
	vector<RenderTexture> ThumbCache;
	//缩略图源图是否已绘制到缓存纹理（避免每帧重复 clear/draw/display）
	vector<bool> ThumbDrawn;
	//缩略图上次绘制时使用的旋转角，用于判断旋转变化后需重建
	int LastRote[12] = { 0 };
	EV ThumbScale[12];
	EV ThumbOffsetY[12];

	//面板内容缩放
	EV xScale;

	//缩略图数量上限
	static const int MAX_THUMB = 9;

	//点击某张缩略图后的回调：跳转到该相册页
	void ChoosePage(int page)
	{
		//相册页索引：0 为展台实时页，缩略图第 i 张对应相册页 i+1
		Write::GoToPage(page);

		//提示用户已选择
		BottomMessage::AddMessage(203, L"已切换到第" + to_wstring(page) + L"张", BottomMessageType_SUCCESS, 180);

		Tool::UpdatePage();

		Tool::UpdateCameraControl();
	}

public:

	//当前展开状态
	bool IsEx = false;

	EV2 pos, size;

	//是否处于展开状态
	bool IsShow() const { return IsEx; }

	void Draw(RenWin& window)
	{
		static IMAGE AlbumIcon;
		static IMAGE CloseIcon;

		//初始化
		if (!init)
		{
			if (!LoadIcon(AlbumIcon, ImgPath + L"Camera\\Images.dll"))
			{
				RightMessage::ShowMessage(L"错误：定位图像Images失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
			}

			if (!LoadIcon(CloseIcon, ImgPath + L"Camera\\CloseCamera.dll"))
			{
				RightMessage::ShowMessage(L"错误：定位图像CloseCamera失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
			}

			//与更多工具栏一致：从窗口底部弹出
			size.x.SetAnimationStartValue(BasicSize * 11 + UISpace);
			size.y.SetAnimationStartValue(BasicSize * 1.5);
			pos.y.SetAnimationStartValue(WindowSize.y);
			pos.x.SetAnimationStartValue(WindowSize.x / 2 - BasicSize * 6);

			//缩放动画初值
			xScale.SetAnimationStartValue(100);

			init = true;
		}

		//与更多工具栏一致：用 xScale 做弹出缩放
		pos.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		pos.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		xScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		//逐张缩略图的浮现动画：前一张展开到一定进度后，下一张才开始
		for (int i = 0; i < MAX_THUMB; i++)
		{
			if (i == 0)
			{
				ThumbScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
				ThumbOffsetY[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
			}
			else if (ThumbScale[i - 1].frame > MinFrame)
			{
				ThumbScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
				ThumbOffsetY[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
			}
		}

		//脏标记：动画未结束时需要重绘
		if (!NeedReDraw)
			NeedReDraw = IsAnimations( pos.y, pos.x, xScale );

		if (!NeedReDraw)
		{
			bool thumbAnimating = false;
			for (int i = 0; i < MAX_THUMB; i++)
			{
				if (ThumbScale[i].IsAnimation() || ThumbOffsetY[i].IsAnimation())
				{
					thumbAnimating = true;
					break;
				}
			}
			NeedReDraw = thumbAnimating;
		}

		//当前相册总张数：CameraPageData 中除实时页(0)外的张数
		int photoCount = Write::TotalPage - 1;
		if (photoCount < 0) photoCount = 0;
		if (photoCount > MAX_THUMB) photoCount = MAX_THUMB;

		//重建缩略图缓存（张数变化时）
		if ((int)ThumbCache.size() != photoCount)
		{
			int StartIndex = (int)ThumbCache.size();
			ThumbCache.clear();
			ThumbCache.resize(photoCount);

			//缩略图数量变化后，源图缓存与旋转角记录必须一并失效，
			//否则新页会复用旧页的旋转角判断，导致方向错误或源图不刷新。
			ThumbDrawn.assign(photoCount, false);
			for (int i = 0; i < 12; i++) LastRote[i] = 0;

			for (int i = StartIndex; i < photoCount; i++)
			{
				ThumbScale[i].SetAnimationStartValue(0);
				ThumbScale[i].SetAnimation(100, TotalFrame);

				ThumbOffsetY[i].SetAnimationStartValue(BasicSize / 2);
				ThumbOffsetY[i].SetAnimation(0, TotalFrame);
			}

			NeedReDraw = true;
		}

		//绘制
		if (NeedReDraw || UpdateUser > 0)
		{
			if (rt.resize(FTU({ size.x.value + 2,size.y.value + 2 })))
			{
				rt.clear(Color::Transparent);

				//绘制底色
				DrawGlassBar(1, 1, size.x.value, size.y.value, RoundSize, rt);

				static float Scale = IconSize / 512.0;

				//入口图标与标题
				XImage::PutScaleImage(AlbumIcon, BasicSize * 0.5, size.y.value * 0.5, Scale, Scale, rt, 0.5, 0.5);

				XText::SetFontConfig(Color::White, FONTSIZE * 0.7);
				XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
				XText::Xyprintf(BasicSize * 0.5, size.y.value - BasicSize * 0.1, L"相册", rt);

				//缩略图列表
				if (photoCount > 0)
				{
					//缩略图尺寸较原尺寸放大约 30%
					static int ThumbSize = BasicSize * 0.9 * 1.3;

					//缩略图源图仅在页数变化或缓存未填充时重建，避免每次重绘都重传纹理
					if ((int)ThumbDrawn.size() != photoCount)
					{
						ThumbDrawn.assign(photoCount, false);
					}

					for (int i = 0; i < photoCount; i++)
					{
						float SubScale = ThumbScale[i].value / 100.0;

						//每张缩略图把对应相册页渲染到独立纹理
						if (ThumbCache[i].getSize().x == 0)
						{
							ThumbCache[i] = RenderTexture(ITU({ ThumbSize,ThumbSize }));
							ThumbDrawn[i] = false;
						}

						// 纹理已绘制过则跳过重绘：缩略图内容只取决于照片与其旋转角，
						// 而 rotate / 换页都会走此处重建，因此缓存命中时无需每帧刷纹理。
						if (!ThumbDrawn[i] || LastRote[i] != WriteCamera::GetRote(i + 1))
						{
							//取该页照片并等比缩放绘制到缩略图纹理
							IMAGE& photo = WriteCamera::GetPhoto(i + 1);

							//读取该页记录的旋转角度，使缩略图与实际显示方向一致
							int rote = WriteCamera::GetRote(i + 1);

							ThumbCache[i].clear(Color(30, 30, 30));

							if (photo.w > 0 && photo.h > 0)
							{
								//旋转为 90/270 度时，绘制后的宽高互换，需按交换后的尺寸计算等比缩放
								bool swap = (abs(rote) % 180 == 90);

								float drawW = swap ? photo.h : photo.w;
								float drawH = swap ? photo.w : photo.h;

								float fit = min(
									(float)ThumbSize / drawW,
									(float)ThumbSize / drawH);

								float ox = drawW * fit * 0.5f;
								float oy = drawH * fit * 0.5f;

								//按旋转角绘制：PutRoteScaleImage 支持旋转+缩放（角度参数在 x,y 之后）
								XImage::PutRoteScaleImage(
									photo, ox, oy, (float)rote,
									fit, fit, ThumbCache[i], 0.5, 0.5);
							}

							ThumbCache[i].display();

							ThumbDrawn[i] = true;
							LastRote[i] = rote;
						}

						//缩略图在面板内水平排布
						float cx = BasicSize * 1.1 + ThumbSize * 0.5 + i * (ThumbSize + BasicSize * 0.15);
						float cy = size.y.value * 0.5 + ThumbOffsetY[i].value;

						Sprite ts(ThumbCache[i].getTexture());
						ts.setPosition({ cx, cy });
						ts.setColor(Color(255, 255, 255, (int)min(255.f, SubScale * 255)));
						ts.setOrigin({ ThumbSize * 0.5f, ThumbSize * 0.5f });
						ts.setScale({ SubScale,SubScale });
						rt.draw(ts);

						//当前正在查看的相册页（缩略图第 i 张对应相册页 i+1）用主色圆角外框标记
						if (Write::Page == i + 1)
						{
							XGraph::SetColor(User::MainColor);
							XGraph::LineShape::SetLineWidth(WindowSize.x / 900);

							float boxSize = ThumbSize * SubScale;
							XGraph::RectangleShape::RoundRect(
								cx - boxSize * 0.5f, cy - boxSize * 0.5f,
								boxSize, boxSize, RoundSize / 4, rt);
						}
					}
				}

				rt.display();
			}
		}

		//鼠标悬停在面板上时禁止书写穿透
		if (IsEx && XMsg::MouseMsg::IsMouseIn(
			pos.x.value, pos.y.value, size.x.value, size.y.value))
		{
			Tool::IsInBar = true;
		}

		//与更多工具栏一致：无动画且已完全收起时跳过绘制
		if (!IsEx && !pos.y.IsAnimation() && pos.y.value >= WindowSize.y - BasicSize) return;

		Sprite s(rt.getTexture());
		
		s.setPosition({ pos.x.value - 1,pos.y.value - 1 });
		s.setColor(Color(255, 255, 255, UIAlpha));
		s.setScale(Vector2f(1, xScale.value / 100.0));
		window.draw(s);

		//按键
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			if (IsEx && photoCount > 0)
			{
				//与绘制保持一致：缩略图放大约 30%，热区随实际绘制尺寸同步
				static int ThumbSize = BasicSize * 0.9 * 1.3;

				for (int i = 0; i < photoCount; i++)
				{
					//与绘制时一致：cx / cy 为缩略图“中心”坐标（绘制时以 0.5,0.5 为原点）
					float cx = BasicSize * 1.1 + ThumbSize * 0.5 + i * (ThumbSize + BasicSize * 0.15);
					float cy = size.y.value * 0.5;

					//IsMouseIn 接收的是左上角，故由中心坐标换算左上角
					if (XMsg::MouseMsg::IsMouseIn(
						pos.x.value - 1 + cx - ThumbSize * 0.5f,
						pos.y.value - 1 + cy - ThumbSize * 0.5f,
						(float)ThumbSize, (float)ThumbSize))
					{
						ChoosePage(i + 1);
						break;
					}
				}
			}
		}

		//自主收回：仅当点击落在面板实际绘制区域之外时才收起
		//注意：面板绘制起点为 pos.x.value - 1，此处判定区域必须与之一致，
		//否则点击靠右的缩略图（大页码）会被误判为“点击外部”而触发收回。
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft) && IsEx && !pos.y.IsAnimation())
		{
			if (!XMsg::MouseMsg::IsMouseIn(
				pos.x.value - 1, pos.y.value - 1,
				size.x.value, size.y.value) && !IsInCamera())
			{
				Exp(false);
			}
		}

		
	}

	//展开/收起相册预览面板
	void Exp(bool Ex)
	{
		//状态未变化时不做无意义动画
		if (IsEx == Ex) return;

		IsEx = Ex;

		if (Ex)
		{
			//相册张数决定所需宽度
			int photoCount = Write::TotalPage - 1;
			if (photoCount < 1) photoCount = 1;
			if (photoCount > MAX_THUMB) photoCount = MAX_THUMB;

			//水平居中、从底部弹出
			size.x.SetAnimation(BasicSize * 11 + UISpace, TotalFrame);
			size.y.SetAnimation(BasicSize * 1.5, TotalFrame);
			pos.x.SetAnimation(WindowSize.x / 2 - BasicSize * 6, TotalFrame);
			pos.y.SetAnimation(WindowSize.y - UISpace * 2 - BasicSize * 1.4 - BasicSize, TotalFrame);

			//展开缩放动画
			xScale.SetAnimationStartValue(ScaleStrgengh);
			xScale.SetAnimation(100, TotalFrame);

			//逐张浮现
			for (int i = 0; i < photoCount; i++)
			{
				ThumbScale[i].SetAnimationStartValue(0);
				ThumbScale[i].SetAnimation(100, TotalFrame);

				ThumbOffsetY[i].SetAnimationStartValue(BasicSize / 2);
				ThumbOffsetY[i].SetAnimation(0, TotalFrame);
			}
		}
		else
		{
			//收起：缩回底部
			pos.x.SetAnimation(WindowSize.x / 2 - BasicSize * 6, TotalFrame);
			pos.y.SetAnimation(WindowSize.y, TotalFrame);
		}

		//设置底部消息偏移
		if (Ex) BottomMessage::SetYOffset(-BasicSize * 1.5 - UISpace);
		else BottomMessage::SetYOffset(0);

		NeedReDraw = true;
	}
};
static CameraPreview CameraPreviewToolBar;

void Tool::ShowPhotoWindow()
{
	if (CameraPreviewToolBar.IsEx) return;

	CameraPreviewToolBar.Exp(true);
}

#pragma endregion

//视频展台
#pragma region MyRegion

class TagBox_Camera
{
	RenderTexture rt;
	bool init = false;

	bool ENABLE_CAMERA = false;

	int exptime = -1;

	void ManageCamera(RenWin& window)
	{
		ENABLE_CAMERA = WriteCamera::Manage();
		PerNeedReDraw = NeedReDraw = true;

		int temp = pos.y.value;

		Tool::IsInBar = true;

		static int MainToolBarTempX = MainToolBar.pos.x.value;
		static int ToolPosTempX = pos.x.value;

		if (ENABLE_CAMERA)
		{
			size.x.SetAnimation(BasicSize * 6,TotalFrame);
			
			MainToolBarTempX = MainToolBar.pos.x.value;
			ToolPosTempX = pos.x.value;

			MainToolBar.pos.x.SetAnimation(MainToolBarTempX - BasicSize * 3,TotalFrame);
			pos.x.SetAnimation(ToolPosTempX - BasicSize * 3, TotalFrame);

			for (int i = 0;i < 5;i++)
			{
				MoreIconScale[i].SetAnimationStartValue(0);
				MoreIconScale[i].SetAnimation(100, TotalFrame);

				MoreIconOffsetY[i].SetAnimationStartValue(BasicSize / 2);
				MoreIconOffsetY[i].SetAnimation(0, TotalFrame);
			}

			ScrollToolBar[0].Hide();
			ScrollToolBar[1].Hide();
		}
		else
		{
			BottomMessage::SetYOffset(0);

			size.x.SetAnimation(BasicSize, TotalFrame);
			MainToolBar.pos.x.SetAnimation(MainToolBarTempX, TotalFrame);
			pos.x.SetAnimation(ToolPosTempX, TotalFrame);

			ScrollToolBar[0].Show();
			ScrollToolBar[1].Show();
		}
	}

	bool LoadFinish = false;

	void Photo()
	{
		if (WriteCamera::EnableAutoPhoto) return;

		WriteCamera::PhotoImage();
	}

	void Set()
	{

	}

	//打开相册照片预览面板
	void Album()
	{
		//至少存在一张相册照片才有预览的必要（相册页从 1 开始）
		if (Write::TotalPage <= 1)
		{
			BottomMessage::AddMessage(203, L"相册为空", BottomMessageType_INFO, 180);
			return;
		}

		//展开预览面板
		CameraPreviewToolBar.Exp(!CameraPreviewToolBar.IsEx);
	}

	void Rote()
	{
		WriteCamera::Rote();
	}

	void Auto()
	{
		WriteCamera::ManageAutoPhoto();
	}

	//缩放
	EV yScale;

public:

	float CenterY = 1;

	bool NeedReDraw = true;
	bool PerNeedReDraw = false;

	EV2 pos, size;

	EV MoreIconScale[5];
	EV MoreIconOffsetY[5];

	void Draw(RenWin& window)
	{
		static IMAGE CameraIcon,CloseCameraIcon,CloseAutoIcon;
		static IMAGE IconMore[5];

		//初始化
		if (!init)
		{
			//图标初始化
			wstring ImgName = L"Camera.dll";

			if (!LoadIcon(CameraIcon, ImgPath + L"Camera\\" + ImgName))
			{
				RightMessage::ShowMessage(L"错误：定位图像Camera失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
			}

			wstring ImgName2 = L"CloseCamera.dll";

			if (!LoadIcon(CloseCameraIcon, ImgPath + L"Camera\\" + ImgName2))
			{
				RightMessage::ShowMessage(L"错误：定位图像CloseCamera失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
			}

			wstring ImgName3 = L"CloseAutoPhoto.dll";

			if (!LoadIcon(CloseAutoIcon, ImgPath + L"Camera\\" + ImgName3))
			{
				RightMessage::ShowMessage(L"错误：定位图像CloseAutoPhoto失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
			}

			//图标初始化
			wstring ImgNameMore[5] = { L"Photo.dll",L"AutoPhoto.dll",L"Rote.dll",L"Images.dll",L"Setting.dll"};

			for(int i = 0;i < 5;i++)
			{
				if (!LoadIcon(IconMore[i], ImgPath + L"Camera\\" + ImgNameMore[i]))
				{
					RightMessage::ShowMessage(L"错误：定位图像Camera.More失败，使用默认图标", L"图标加载", RightMessageType_ERROR, true);
				}
			}

			size.x.SetAnimationStartValue(BasicSize);
			size.y.SetAnimationStartValue(BasicSize);

			pos.x.SetAnimationStartValue(WindowSize.x / 2 - BasicSize * 0.5);
			MainToolBar.pos.x.SetAnimation(MainToolBar.pos.x.value - BasicSize * 0.5, TotalFrame);

			pos.y.SetAnimationStartValue(WindowSize.y + BasicSize);
			pos.y.SetAnimation(WindowSize.y - UISpace,TotalFrame);

			//设置缩放动画
			yScale.SetAnimationStartValue(100);

			NeedReDraw = true;

			init = true;
		}

		size.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		//脏标记
		if (!NeedReDraw)
			NeedReDraw = IsAnimations(
				size.x,size.y,MoreIconScale[0],MoreIconScale[1],MoreIconScale[2],MoreIconScale[3],MoreIconScale[4]
				);

		//动画更新
		pos.x.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		for(int i = 0;i < 5;i++)
		{
			if(i == 0)
			{
				MoreIconScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
				MoreIconOffsetY[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
			}
			else if(MoreIconScale[i - 1].frame > MinFrame)
			{
				MoreIconScale[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
				MoreIconOffsetY[i].UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
			}
		}

		if(MainToolBar.pos.y.frame > MinFrame)
		{
			pos.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
		}

		//绘制
		if (NeedReDraw || UpdateUser > 0)
		{
			if (rt.resize(FTU({ size.x.value + 2,size.y.value + 2 })))
			{
				rt.clear(Color::Transparent);

				//绘制底色
				DrawGlassBar(1, 1, size.x.value, size.y.value, RoundSize, rt);

				static float Scale = IconSize / 512.0;

				//绘制图片
				if(WriteCamera::IsRun())
				{
					XImage::PutScaleImage(CloseCameraIcon, BasicSize / 2, BasicSize / 2, Scale, Scale, rt, 0.5, 0.5);

					//绘制文字
					static wstring text = L"关闭视频展台";

					static int FontSize = FONTSIZE * 0.8;
					XText::SetFontConfig(Color::White, FontSize);
					XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
					XText::Xyprintf(BasicSize / 2, size.y.value - BasicSize * 0.1, text, rt);

					XText::SetFontConfig(Color::White, FONTSIZE);
					for (int i = 0;i < 5;i++)
					{
						static wstring name[5] = { L"拍照",L"自动拍照",L"旋转",L"相册",L"展台设置"};

						if (Write::Page > 0) name[0] = L"返回展台";
						else name[0] = L"拍照";

						if (WriteCamera::EnableAutoPhoto) name[1] = L"关闭自动拍照";
						else name[1] = L"自动拍照";

						float SubScale = MoreIconScale[i].value / 100.0;

						if(i != 1 || !WriteCamera::EnableAutoPhoto)
						{
							XImage::PutScaleImage(
								IconMore[i], BasicSize * (i + 1) + BasicSize / 2, BasicSize / 2 + MoreIconOffsetY[i].value,
								Scale * SubScale, Scale * SubScale, rt, 0.5, 0.5);
						}
						else
						{
							XImage::PutScaleImage(
								CloseAutoIcon, BasicSize* (i + 1) + BasicSize / 2, BasicSize / 2 + MoreIconOffsetY[i].value,
								Scale* SubScale, Scale* SubScale, rt, 0.5, 0.5);
						}

						int alpha = min(255, SubScale * 255);
						XText::SetFontConfig(Color(255,255,255, alpha), FontSize);
						XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
						XText::Xyprintf(BasicSize * (i + 1) + BasicSize / 2, size.y.value - BasicSize * 0.1, name[i], rt);
					}
				}
				else
				{
					XImage::PutScaleImage(CameraIcon, BasicSize / 2, BasicSize / 2, Scale, Scale, rt, 0.5, 0.5);

					//绘制文字
					static wstring text = L"视频展台";

					static int FontSize = FONTSIZE;
					XText::SetFontConfig(Color::White, FontSize);
					XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
					XText::Xyprintf(BasicSize / 2, size.y.value - BasicSize * 0.1, text, rt);
				}

				rt.display();
			}
		}

		//按键
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			if (XMsg::MouseMsg::IsMouseIn(pos.x.value - 1 + MainToolBar.size.x.value / 2 + UISpace, pos.y.value - CenterY * size.y.value, BasicSize, size.y.value))
			{
				//最小化
				ManageCamera(window);
			}

			if (ENABLE_CAMERA)
			{
				for (int i = 0;i < 5;i++)
				{
					if (XMsg::MouseMsg::IsMouseIn(
						pos.x.value - 1 + MainToolBar.size.x.value / 2 + UISpace + BasicSize * (i + 1),
						pos.y.value - CenterY * size.y.value, 
						BasicSize, size.y.value))
					{
						//拍照
						if (i == 0)
						{
							Photo();
						}
						//自动拍照
						if (i == 1)
						{
							Auto();
						}
						//旋转
						if (i == 2)
						{
							Rote();
						}
						//相册
						if (i == 3)
						{
							Album();
						}
						//设置
						if (i == 4)
						{
							Set();
						}

						MoreIconScale[i].SetAnimationStartValue(30);
						MoreIconScale[i].SetAnimation(100, TotalFrame);
					}
				}
			}
		}

		if (XMsg::MouseMsg::IsMouseIn(
			pos.x.value - 1 + MainToolBar.size.x.value / 2 + UISpace, pos.y.value - size.y.value * CenterY, size.x.value, size.y.value))
		{
			Tool::IsInBar = true;
		}

		if (pos.y.frame > MinFrame) yScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);

		Sprite s(rt.getTexture());
		
		s.setPosition({ pos.x.value - 1 + MainToolBar.size.x.value / 2 + UISpace,pos.y.value - 1 - CenterY * size.y.value });
		s.setColor(Color(255, 255, 255, UIAlpha));
		s.setScale(Vector2f(1, yScale.value / 100.0));
		window.draw(s);

		//自动回放检测
		if (exptime > -1)
		{
			exptime -= 1;
			if (exptime == 0)
			{
				static int ExpPos = WindowSize.y - UISpace;
				pos.y.SetAnimation(ExpPos, TotalFrame);

				yScale.SetAnimationStartValue(ScaleStrgengh);
				yScale.SetAnimation(100, TotalFrame);
			}
		}

		if (!PerNeedReDraw) NeedReDraw = false;
		else PerNeedReDraw = false;
	}
	void SetX(int x, int TotalFrame)
	{
		pos.x.SetAnimation(x, TotalFrame);
	}

	void exp()
	{
		exptime = EXP_TIME;

		static int ExpPos = WindowSize.y - UISpace + BasicSize * 0.9;
		if (pos.y.IsAnimation() || pos.y.value >= ExpPos) return;

		pos.y.SetAnimation(ExpPos, TotalFrame);
	}
};
static TagBox_Camera Tag_CameraToolBar;

void Tool::UpdateCameraControl()
{
	Tag_CameraToolBar.NeedReDraw = true;
}

bool IsInCamera()
{
	return 
		XMsg::MouseMsg::IsMouseIn(
			Tag_CameraToolBar.pos.x.value - 1 + MainToolBar.size.x.value / 2 + UISpace, 
			Tag_CameraToolBar.pos.y.value - 1 - Tag_CameraToolBar.CenterY * Tag_CameraToolBar.size.y.value,
			Tag_CameraToolBar.size.x.value, 
			Tag_CameraToolBar.size.y.value);
}

#pragma endregion




#pragma endregion

void Tool::Draw(RenWin& window)
{
	Tool::IsInBar = false;

	//初始化
	static bool init = false;
	if (!init)
	{

		BasicSize = WindowSize.x / 21;
		UISpace = WindowSize.x * 0.01;
		RoundSize = WindowSize.x / 100;

		IconSize = BasicSize * 0.7;

		TotalFrame = 30;
		MinFrame = TotalFrame / 6;

		for (int i = 0; i < 4; i++)
		{
			PerSetPenSize[i] = (i + 1) * 3 * ScreenSize.x / 1920;
		}

		Tool::PenSize = PerSetPenSize[1];

		ScrollToolBar[1].left = false;

		init = true;
	}

	//启动时间
	static int StartClock = 120;
	if (StartClock > 0) StartClock -= 1;

	//启用右侧消息
	if(StartClock == 1) RightMessage::SetVisible(true);

	//插件
	ExtToolBar.Draw(window);

	//绘制更多工具栏
	MoreToolBar.Draw(window);
	
	//绘制滑动工具栏-左
	if (StartClock < 60) ScrollToolBar[0].Draw(window);
	//绘制菜单栏
	if (StartClock < 60) StartToolBar.Draw(window);
	//绘制最小化栏
	if (StartClock < 56) MinToolBar.Draw(window);
	//绘制底部工具栏
	if (StartClock < 40) MainToolBar.Draw(window);
	//绘制页面管理工具栏
	if (StartClock < 30) PageToolBar.Draw(window);
	//绘制滑动工具栏-右
	if (StartClock < 20) ScrollToolBar[1].Draw(window);
	//绘制固定项
	if (StartClock <= 0) Tag_CameraToolBar.Draw(window);
	//绘制相册照片预览（仅展台开启时可用）
	if (StartClock <= 0 && WriteCamera::IsRun()) CameraPreviewToolBar.Draw(window);
	

	//检查
	if (NeedEnterGet) Get(window);
	if (NeedEnterMin) MinExe(window);
	if (NeedEnterGeometricLibrary) EnterGeometricLibrary(window);
	if (NeedEnterTerminal) EnterTerminal(window);
	if (NeedEnterSave) EnterSaveOrOpen(window);
	if (NeedEnterChoosePage) EnterChoosePage(window);
	if (NeedEnterSetting) EnterSetting(window);
}

void Tool::Exp::ExpBottomToolBar()
{
	MainToolBar.exp();
	Tag_CameraToolBar.exp();
}

void Tool::Exp::ExpMinBar()
{
	MinToolBar.exp();
}

void Tool::Exp::ExpStartBar()
{
	StartToolBar.exp();
}

void Tool::Exp::ExpPageBar()
{
	PageToolBar.exp();
}

void Tool::Exp::ExpLeftScrollBar()
{
	ScrollToolBar[0].exp();
}

void Tool::Exp::ExpRightScrollBar()
{
	ScrollToolBar[1].exp();
}

void Tool::UpdatePage()
{
	UpdatePageBar();
}