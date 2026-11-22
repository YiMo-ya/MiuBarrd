#include "AI.h"
#include "RightMessage.h"
#include "Shared.h"
#include "Tool.h"
#include "Debug.h"
#include <deque>
#include <random>

NOXS; NOSTD;

static bool LoadSuccess = true;

//状态
static int AIState = 0;
static int LastState = 0;

static Vector2i Pos;

//休息计时
static int SatClock = 0;
//睡眠计时
static int SleepClock = 0;
//晕厥计时
static int MoveFastClock = 0;

vector<wstring> says;
int SayAlpha = 0;
int SayClock = 0;

RenderTexture SayTemp;

static // 按视觉占位长度分割宽字符串，汉字等 CJK 字符计 2，ASCII 计 1
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

static void DrawSay(RenWin& window)
{
	if (says.empty()) return;

	if (SayClock > 0) SayClock -= 1;
	if (SayClock == 0)
	{
		if (SayAlpha - 15 > 0) SayAlpha -= 15;
		else
		{
			SayAlpha = 0;
			says.clear();
		}
	}
	else
	{
		if (SayAlpha + 25 < 255) SayAlpha += 25;
		else SayAlpha = 255;
	}

	//计算坐标
	static int Size = WindowSize.x * 0.03;
	static int w = WindowSize.x * 0.1;
	int h = WindowSize.y * 0.03 * says.size();
	int x = Pos.x - w / 2;
	int y = Pos.y - Size - h * 1.3;

	Sprite s(SayTemp.getTexture());
	s.setPosition({ (float)x,(float)y });
	s.setColor(Color(255, 255, 255, SayAlpha));
	window.draw(s);
}

void Say(wstring text)
{
	says.clear();
	says = SplitWStringByWidth(text, 20);

	static int Size = WindowSize.x * 0.03;

	//计算坐标
	static int w = WindowSize.x * 0.1;
	int h = WindowSize.y * 0.03 * says.size();
	int x = Pos.x - w / 2;
	int y = Pos.y - Size - h * 1.3;

	static int r = w * 0.1;

	SayTemp.resize({ unsigned int(w),unsigned int(h) });
	SayTemp.clear(Color(0, 0, 0, 0));

	XGraph::SetFillColor(Color(50, 50, 50));
	XGraph::RectangleShape::FillRoundRect_WithoutBorder(0, 0, w, h, r, SayTemp);

	static int fsize = WindowSize.x / 100;
	XText::SetFontConfig(Color(255, 255, 255), fsize);
	XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
	for (int i = 0; i < says.size(); i++)
	{
		XText::Xyprintf(w / 2, WindowSize.y * 0.03 / 2 + WindowSize.y * 0.03 * i, says[i], SayTemp);
	}

	SayTemp.display();

	SayAlpha = 0;
	SayClock = 150;
}


//语言拟态
#pragma region MyRegion

static int Random(int min, int max) {
	static thread_local std::mt19937 gen(std::random_device{}());
	std::uniform_int_distribution<int> dist(min, max);
	return dist(gen);
}

void SayHello()
{
	vector<wstring> HelloText = {
		// 打招呼
		L"你好呀，我是小Miu，有问题随时可以找我  (p > w < q)",
		L"嗨！我在呢，有什么可以帮你的吗？",
		L"你来了！等你好久了 (*/ω＼*)",
		L"哟呼~ 今天过得怎么样呀？",
		L"叮咚！小Miu已上线，请多关照~",

		// 唱歌
		L"[唱歌] 在那山的那边海的那边有一群蓝精灵...",
		L"[唱歌] 快乐的池塘边有一群小青蛙...",
		L"[唱歌] 小星星亮晶晶，满天都是小星星~",
		L"[唱歌] 一闪一闪亮晶晶，好像许多小眼睛 ",
		L"[哼歌] 啦啦啦~ 啦啦啦~ 我是快乐的小Miu~",

		// 乖巧回应
		L"有什么事情吗？",
		L"我在呢。",
		L"我一直都在这里哦，随时等你叫我~",
		L"嗯哼？想跟我说点什么呀？",
		L"我在，你说",
		L"随时待命中",

		// 小提示 & 彩蛋
		L"告诉你个秘密，其实双指可以缩放移动",
		L"悄悄说：你可以拖拽我到屏幕任意位置",

		// 自我介绍
		L"我是小Miu",
		L"我是小Miu，你的板书助手~",
		L"小Miu在此，有何贵干？(抱拳)",
		L"我叫小Miu，今年刚出厂，还请多多指教~",

		// 位置/存在感
		L"我在这里",
		L"我就在屏幕上",

		L"秦孝公据崤函之固，拥雍州之地 ... ...，忘记了 ... ...",
		L"床前明月光，疑是地上霜……后面是啥来着？",
		L"春眠不觉晓，处处蚊子咬... 好像哪里不对？",
	};

	int index = Random(0, HelloText.size() - 1);

	Say(HelloText[index]);
}

void SayHelloFast()
{
	vector<wstring> Text = {
		L"天上全是星星 ... ...",
		L"脑袋要炸了",
		L"1 + 2 = ... 自检失败",
		L"还在旋转吗?",
		L"我要倒了... ...",
		L"还在摇吗... ...",
		L"停下来了... ...",
		L"警告：小Miu现在CPU占用率极高!"
	};

	int index = Random(0, Text.size() - 1);

	Say(Text[index]);
}

void SayNoMove()
{
	vector<wstring> Text = {
		L"不要摇了... ...",
		L"过山车来的... ...",
		L"太快了... ...",
		L"别摇了，我要吐了 >_<",
		L"停下来... ...",
	};

	int index = Random(0, Text.size() - 1);

	Say(Text[index]);
}

void SayMoveFastRestore()
{
	vector<wstring> Text = {
		L"小Miu恢复正常!",
		L"自我修复成功",
		L"终于不晕了",
		L"还好没吐",
		L"终于缓过来了",
	};

	int index = Random(0, Text.size() - 1);

	Say(Text[index]);
}

#pragma endregion


static void DrawAI(RenWin& window)
{
	//图像定义
	static vector<IMAGE> Model;
	static EV ImgScale;

	static bool init = false;
	if (!init)
	{
		static vector<wstring> ModelName = {L"Normal.mdl",L"Sat.mdl",L"Move.mdl",L"Sleep.mdl",L"MoveFast.mdl",L"MoveFast2.mdl"};
		for (int i = 0; i < ModelName.size(); i++)
		{
			Model.emplace_back();
			if (!XImage::NewImage(Model.back(), ImgPath + L"AI\\" + ModelName[i]))
			{
				LoadSuccess = false;
				RightMessage::ShowMessage(L"初始化AI失败，加载图像模型错误", L"初始化AI失败 ", RightMessageType_ERROR, true);
			}
		}

		Pos.x = WindowSize.x * 0.95;
		Pos.y = WindowSize.y * 0.85;

		ImgScale.SetAnimation(100, 30);

		AIState = 1;

		init = true;
	}

	ImgScale.UpdateAnimation(XEase::EaseBasic::easeOutBack, 3);

	static int Size = WindowSize.x * 0.03;

	static float BasicScale = Size / 512.0;
	float Scale = BasicScale * ImgScale.value / 100.0;

	static int StateTemp = AIState;
	if (StateTemp != AIState)
	{
		if(AIState != 3)
		{
			ImgScale.SetAnimationStartValue(80);
		}

		ImgScale.SetAnimation(100, 30);

		StateTemp = AIState;
	}
	if (!ImgScale.IsAnimation())
	{
		LastState = AIState;
	}

	if (SatClock > 0) SatClock -= 1;
	if (SatClock == 1) AIState = 1;

	if (SleepClock < 1800) SleepClock += 1;
	if (SleepClock == 1799) AIState = 3;

	if (MoveFastClock > 0) MoveFastClock -= 1;
	if (MoveFastClock == 1)
	{
		ImgScale.SetAnimationStartValue(80);
		ImgScale.SetAnimation(100, 30);
		SayMoveFastRestore();
	}

	//绘制图像模型
	if(MoveFastClock == 0) XImage::PutScaleImage(Model[AIState], Pos.x, Pos.y, Scale, Scale, window, 0.5, 0.5);
	else XImage::PutScaleImage(Model[AIState + 3], Pos.x, Pos.y, Scale, Scale, window, 0.5, 0.5);
	//睡眠绘制
	if (AIState == 3)
	{
		if(ImgScale.IsAnimation())
		{
			Model[LastState].color.a = 255 - 255 * ImgScale.frame / ImgScale.totalframe;
			XImage::PutScaleImage(Model[LastState], Pos.x, Pos.y, Scale, Scale, window, 0.5, 0.5);
			Model[LastState].color.a = 255;
		}

		static int ZAlpha;
		ZAlpha += 3;
		if (ZAlpha > 255) ZAlpha = 0;

		int Alpha1 = ZAlpha;
		int Alpha2 = ZAlpha - 130;
		if (Alpha2 < 0) Alpha2 += 255;

		static int fsize = WindowSize.x / 70;
		XText::SetFontConfig(Color(255, 255, 255, Alpha1), fsize);
		XText::Xyprintf(Pos.x + Size * 0.5, Pos.y - Size * 1.2,L"z",window);
		XText::SetFontConfig(Color(255, 255, 255, Alpha2), fsize);
		XText::Xyprintf(Pos.x + Size * 0.7, Pos.y - Size * 1.5, L"Z", window);
	}

	bool ClickHot = false;
	static int ClickClock = 0; //点击时间
	static int FSC = 0; //晕厥计数
	if (XMsg::KeyMsg::Keystate(VK::MouseLeft))
	{
		if(ClickHot == false)
		{
			if (XMsg::MouseMsg::IsMouseIn(Pos.x - Size, Pos.y - Size, Size * 2, Size * 2))
			{
				if (ClickClock == 0)
				{
					ImgScale.SetAnimationStartValue(80);
					ImgScale.SetAnimation(100, 30);
				}

				if (ClickClock < 5) ClickClock += 1;
				else
				{
					AIState = 2;
				}

				SleepClock = 0;

				Tool::IsInBar = true;
			}

			if (AIState == 2)
			{
				Vector2i NewPos = XMsg::MouseMsg::GetMousePosWindow();

				static int MaxSS = WindowSize.x / 25;
				if (abs(NewPos.x - Pos.x) > MaxSS || abs(NewPos.y - Pos.y) > MaxSS)
				{
					FSC += 1;
					if(FSC == 6) SayNoMove();
				}
				else
				{
					if(FSC > 0) FSC -= 1;
				}
				if (FSC > 5)
				{
					MoveFastClock = 600;
				}

				Pos = NewPos;

				Tool::IsInBar = true;
			}

			ClickHot = true;
		}
	}
	else
	{
		if(ClickClock > 0)
		{
			if(ClickClock >= 5)
			{
				AIState = 1;
				//SatClock = 240;
			}

			if (ClickClock < 5)
			{
				if (MoveFastClock == 0) SayHello();
				else SayHelloFast();
			}

			ClickClock = 0;

			
		}
		ClickHot = false;
		FSC = 0;
	}

	if(Debug)
	{
		XGraph::SetColor(Color(255, 255, 100));
		XGraph::LineShape::SetLineWidth(3);
		XGraph::RectangleShape::Rect(Pos.x - Size, Pos.y - Size, Size * 2, Size * 2, window);
	}
}

void AI::Draw(RenWin& window)
{
	if (!LoadSuccess) return;

	DrawAI(window);
	DrawSay(window);
}