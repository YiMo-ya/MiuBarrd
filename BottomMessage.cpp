#include "BottomMessage.h"
#include "Shared.h"
#include "User.h"
#include <deque>

NOXS; NOSTD;

Color BottomMessageColor_Border[4] = {
	Color::White,Color(255,100,100),Color(100,255,100),Color::White
};

struct BottomMessageStruct
{
	EV y;
	wstring Text;
	int Time;
	int TotalTime;

	BottomMessageType type;

	int id;
};

deque<BottomMessageStruct> BottomMessages;

static int BasicSize;
static int UISpace;

static int BottomMessageBoxH;
static int BottomMessageBoxW;

EV BottomMessageYOffset;

//推入消息
void BottomMessage::AddMessage(int id,wstring text, BottomMessageType type, int time)
{
	if (!BottomMessages.empty())
	{
		BottomMessages.pop_front();
	}

	EV y;
	y.SetAnimationStartValue(WindowSize.y);
	y.SetAnimation(WindowSize.y - BottomMessageBoxH - UISpace * 2 - BasicSize);
	BottomMessages.push_back({ y,text,time,time,type,id });
}

//处理消息
void BottomMessage::MessageManage(RenWin& window)
{
	//初始化
	static bool init = false;
	if (!init)
	{
		BasicSize = WindowSize.x / 20;
		UISpace = WindowSize.x * 0.01;

		BottomMessageBoxH = BasicSize / 2;
		BottomMessageBoxW = BasicSize * 2;

		BottomMessageColor_Border[0] = BottomMessageColor_Border[3] = User::MainColor;

		init = true;
	}

	BottomMessageYOffset.UpdateAnimation(XEase::EaseBasic::easeOut, 4);

	//标准y
	static int BasicY = WindowSize.y - BottomMessageBoxH - UISpace * 2 - BasicSize;

	if (BottomMessages.empty()) return;

	auto& msg = BottomMessages.front();

	//时间处理
	if (msg.Time > 0) msg.Time -= 1;
	
	if(msg.Time == 1)
	{
		if(!msg.y.IsAnimation()) msg.y.SetAnimation(WindowSize.y);
	}

	//擦除msg
	if (msg.Time <= 0 && !msg.y.IsAnimation())
	{
		BottomMessages.pop_front();
		return;
	}

	//动画处理
	if (msg.Time > 0) msg.y.UpdateAnimation(XEase::EaseBasic::easeOutBack, 1.5);
	else msg.y.UpdateAnimation(XEase::EaseBasic::easeIn, 4);

	//在窗口外不绘制
	static int ShowY = WindowSize.y - BasicSize * 1.5;
	if (msg.y.value > ShowY) return;
	
	XGraph::SetFillColor(Color(25, 25, 25, 100));
	XGraph::SetColor(BottomMessageColor_Border[msg.type]);

	//静态边框大小
	static int BorderWidth = WindowSize.x / 1200;
	XGraph::LineShape::SetLineWidth(BorderWidth);

	//静态x坐标
	static int x = WindowSize.x / 2 - BasicSize;

	//静态圆角半径
	static int r = BottomMessageBoxH / 4;

	//绘制
	XGraph::RectangleShape::FillRoundRect(x, msg.y.value + BottomMessageYOffset.value, BottomMessageBoxW, BottomMessageBoxH, r, window);

	//静态文字大小
	static int FontSize = FONTSIZE * 1.1;

	//绘制文字
	XText::SetFontConfig(Color::White, FontSize);
	XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
	XText::Xyprintf(x + BottomMessageBoxW / 2, msg.y.value + BottomMessageBoxH / 2 + BottomMessageYOffset.value, msg.Text, window);

	//绘制加载图标
	if (msg.type == BottomMessageType_LOAD)
	{
		//绘制加载图标
	}
}

void BottomMessage::UpdateCurrentMessageType(BottomMessageType type)
{
	if (BottomMessages.empty()) return;

	auto& msg = BottomMessages.front();
	msg.type = type;
}

void BottomMessage::UpdateCurrentMessageText(wstring text)
{
	if (BottomMessages.empty()) return;

	auto& msg = BottomMessages.front();
	msg.Text = text	;
}

void BottomMessage::UpdateCurretnMessageTime(int time)
{
	if (BottomMessages.empty()) return;

	auto& msg = BottomMessages.front();
	msg.Time = time;
	if (msg.Time > msg.TotalTime) msg.TotalTime = msg.Time;
	msg.y.SetAnimation(WindowSize.y - BottomMessageBoxH - UISpace * 2 - BasicSize);
}

void BottomMessage::ResetY()
{
	if (BottomMessages.empty()) return;

	auto& msg = BottomMessages.front();
	msg.y.SetAnimationStartValue(WindowSize.y);
	msg.y.SetAnimation(WindowSize.y - BottomMessageBoxH - UISpace * 2 - BasicSize);
}

bool BottomMessage::IsMessageNow(int id)
{
	if (BottomMessages.empty()) return false;
	return BottomMessages.front().id == id;
}

void BottomMessage::SetYOffset(int value)
{
	BottomMessageYOffset.SetAnimation(value, 30);
}