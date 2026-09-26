#include "RightMessage.h"
#include "Shared.h"
#include "User.h"
#include "SoundPlayer.h"
#include <deque>

NOXS; NOSTD;

const int TotalShowTime = 5 * 60;

Color RightMessageColor_Border[4] = {
	Color::White,Color(255,100,100),Color(100,255,100),Color(255,255,150)
};

struct RightMessageStruct
{
	EV x;
	vector<wstring> Text;
	wstring Title;
	int Time;
	IMAGE img;

	RightMessageType type;

	bool p;
};

deque<RightMessageStruct> RightMessages;

static int BasicSize;
static int UISpace;

//处理消息
static bool RightMessageVisible = true;
void RightMessage::Manage(RenWin& window)
{
	if (!RightMessageVisible) return;

	//初始化
	static bool init = false;
	if (!init)
	{
		BasicSize = WindowSize.x / 21;
		UISpace = WindowSize.x * 0.01;
		RightMessageColor_Border[0] = User::MainColor;
	}
	init = true;

	if (RightMessages.empty()) return;

	auto& msg = RightMessages.front();

	//时间处理
	if (msg.Time > 0) msg.Time -= 1;

	if (msg.Time == TotalShowTime - 10)
	{
		if (msg.type == RightMessageType_INFO) player.Play(L"info");
		if (msg.type == RightMessageType_WARNING) player.Play(L"warning");
		if (msg.type == RightMessageType_ERROR) player.Play(L"error");
		if (msg.type == RightMessageType_SUCCESS) player.Play(L"success");
	}
	if (msg.Time == 1)
	{
		if (!msg.x.IsAnimation()) msg.x.SetAnimation(WindowSize.x);
	}

	//擦除msg
	if (msg.Time <= 0 && !msg.x.IsAnimation())
	{
		RightMessages.pop_front();
		return;
	}

	//动画处理
	if (msg.Time > 0) msg.x.UpdateAnimation(XEase::EaseBasic::easeOut, 10);
	else msg.x.UpdateAnimation(XEase::EaseBasic::easeIn, 10);

	//在窗口外不绘制
	if (msg.x.value > WindowSize.x) return;

	XGraph::SetFillColor(Color(25,25,25,100));
	XGraph::SetColor(RightMessageColor_Border[msg.type]);

	//静态边框大小
	static int BorderWidth = WindowSize.x / 1200;
	XGraph::LineShape::SetLineWidth(BorderWidth);

	//静态大小
	static int BarW = BasicSize * 3;
	static int BarH = WindowSize.y / 3.5;

	//静态y坐标
	static int y = WindowSize.y - BasicSize - UISpace * 2 - BarH;

	//静态圆角半径
	static int r = BarH / 20;

	//绘制
	XGraph::RectangleShape::FillRoundRect(msg.x.value, y, BarW, BarH, r, window);

	//绘制图片
	static float Scale = BarW * 0.45 / 512.0;
	XImage::PutScaleImage(msg.img,msg.x.value + BarW * 0.5, y, Scale, Scale, window,0.5,0);

	//绘制标题
	static int FontSize = FONTSIZE * 1.3;
	XText::SetFontConfig(Color::White, FontSize);
	XText::SetFontAdjust(ADJUST_LEFT, ADJUST_TOP);
	XText::Xyprintf(msg.x.value + BarW * 0.05, y + BarH * 0.45 + BarH * 0.02, msg.Title, window);

	//绘制说明
	XText::SetFontConfig(Color::White, FONTSIZE);
	for(int i = 0;i<msg.Text.size();i++)
	{
		int texty = y + BarH * 0.6 + BarH * i * 0.06;
		XText::Xyprintf(msg.x.value + BarW * 0.05, texty, msg.Text[i], window);
	}
}

// 按视觉占位长度分割宽字符串，汉字等 CJK 字符计 2，ASCII 计 1
vector<wstring> SplitWStringByWidth(const wstring& input, int maxLen)
{
	vector<wstring> result;
	if (input.empty()) return result;

	wstring current;
	int curLen = 0;

	for (wchar_t c : input)
	{
		// 换行符：强制断开
		if (c == L'\n')
		{
			result.push_back(current);
			current.clear();
			curLen = 0;
			continue;
		}

		// CJK 统一表意字符范围：计 2
		int charLen = 0;
		if (c >= 0x4E00 && c <= 0x9FFF)       // CJK 基本
			charLen = 2;
		else if (c >= 0x3400 && c <= 0x4DBF)  // CJK 扩展 A
			charLen = 2;
		else if (c >= 0x20000 && c <= 0x2A6DF)// CJK 扩展 B
			charLen = 2;
		else if (c >= 0xFF00 && c <= 0xFFEF)  // 全角 ASCII（FF01~FF5E）
			charLen = 2;
		else if (c < 0x80)                     // ASCII
			charLen = 1;
		else                                   // 其他（符号、拉丁扩展等）默认 1
			charLen = 1;

		// 超长则断行
		if (curLen + charLen > maxLen && !current.empty())
		{
			result.push_back(current);
			current.clear();
			curLen = 0;
		}

		current += c;
		curLen += charLen;
	}

	if (!current.empty())
		result.push_back(current);

	return result;
}

void RightMessage::ShowMessage(wstring text, wstring title, RightMessageType type,bool important)
{
	if (!RightMessages.empty())
	{
		if(!RightMessages.front().p)
		RightMessages.pop_front();
	}

	static int BarW = WindowSize.x * 3 / 21;

	EV x;
	x.SetAnimationStartValue(WindowSize.x);
	x.SetAnimation(WindowSize.x - BarW - WindowSize.x * 0.01,45);
	RightMessages.push_back({ x,SplitWStringByWidth(text,28),L"注意：" + title,TotalShowTime,IMAGE(),type,important });

	auto& msg = RightMessages.back();
	if (msg.type == RightMessageType_INFO) XImage::NewImage(msg.img, ImgPath + L"\\Msg\\Info.dll");
	if (msg.type == RightMessageType_WARNING) XImage::NewImage(msg.img, ImgPath + L"\\Msg\\Warning.dll");
	if (msg.type == RightMessageType_ERROR) XImage::NewImage(msg.img, ImgPath + L"\\Msg\\Error.dll");
	if (msg.type == RightMessageType_SUCCESS) XImage::NewImage(msg.img, ImgPath + L"\\Msg\\Success.dll");
	if (RightMessageColor_Border[0] == Color::White) RightMessageColor_Border[0] = User::MainColor;
	msg.img.color = RightMessageColor_Border[msg.type];
}

void RightMessage::SetVisible(bool v)
{
	RightMessageVisible = v;
}