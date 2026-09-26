#include "Update.h"
#include "Shared.h"
#include "User.h"
#include "RightMessage.h"
#include <random>

NOXS; NOSTD;

string ver;

//更新
#pragma region MyRegion

bool down = false;
class PutTextSay
{
private:
	// 线性插值两个Color
	static Color LerpColor(const Color& c1, const Color& c2, float t) {
		return Color(
			static_cast<int>(c1.r + (c2.r - c1.r) * t),
			static_cast<int>(c1.g + (c2.g - c1.g) * t),
			static_cast<int>(c1.b + (c2.b - c1.b) * t),
			static_cast<int>(c1.a + (c2.a - c1.a) * t)
		);
	}

	// 获取渐变色
	static Color GetGradientColor(const std::vector<Color>& colorList, float t) {
		if (colorList.empty()) return Color(255, 255, 255, 255);
		if (colorList.size() == 1) return colorList[0];
		float seg = 1.0f / (colorList.size() - 1);
		int idx = static_cast<int>(t / seg);
		if (idx >= colorList.size() - 1) idx = colorList.size() - 2;
		float localT = (t - idx * seg) / seg;
		return LerpColor(colorList[idx], colorList[idx + 1], localT);
	}

public:

	// 按UTF-8字符分割字符串
	static std::vector<std::string> Utf8Split(const std::string& str) {
		std::vector<std::string> result;
		size_t i = 0;
		while (i < str.size()) {
			unsigned char c = str[i];
			size_t charLen = 1;
			if ((c & 0x80) == 0x00) charLen = 1;           // 1字节ASCII
			else if ((c & 0xE0) == 0xC0) charLen = 2;      // 2字节
			else if ((c & 0xF0) == 0xE0) charLen = 3;      // 3字节
			else if ((c & 0xF8) == 0xF0) charLen = 4;      // 4字节
			if (i + charLen > str.size()) break;           // 防止越界
			result.push_back(str.substr(i, charLen));
			i += charLen;
		}
		return result;
	}

	static void Draw(
		int x, int y,
		const std::string& text,
		const std::vector<Color>& colorList,
		int frame, int totalFrame,
		int fadeInFrame, int fontsize, RenderWindow& window
	) {
		auto chars = Utf8Split(text);
		int len = static_cast<int>(chars.size());
		if (len == 0) return;

		// 1. 计算总宽度
		int totalWidth = 0;
		std::vector<int> charWidths(len);
		for (int i = 0; i < len; ++i) {
			unsigned char first = static_cast<unsigned char>(chars[i][0]);
			charWidths[i] = (first <= 0x7F) ? (fontsize * 2 / 3) : fontsize;
			totalWidth += charWidths[i];
		}

		int drawX = x - totalWidth / 2;
		int intervalFrame = max(1, (totalFrame - fadeInFrame) / (len + 2));

		for (int i = 0; i < len; ++i) {
			int charAlpha = 255;
			float FontSize = (float)fontsize; // 默认正常字号

			if (!down) {
				// ---- 滑入：0 → fontsize ----
				int charFrame = frame - i * intervalFrame;

				if (charFrame <= 0) {
					FontSize = 0.0f;
					charAlpha = 0;
				}
				else if (charFrame < fadeInFrame) {
					float t = std::clamp(charFrame / float(fadeInFrame), 0.0f, 1.0f);
					float easeT = XEase::EaseBasic::easeOutBack(t,2.5);
					FontSize = fontsize * easeT;
					charAlpha = static_cast<int>(255.0f * easeT);
				}
				// else → FontSize = fontsize（默认）
			}
			else {
				// ---- 滑出：fontsize → 0 ----
				int outFrame = totalFrame - frame;
				int charOutFrame = outFrame - i * intervalFrame;

				if (charOutFrame <= 0) {
					FontSize = (float)fontsize;
					charAlpha = 255;
				}
				else if (charOutFrame < fadeInFrame) {
					float t = std::clamp(charOutFrame / float(fadeInFrame), 0.0f, 1.0f);
					float easeT = XEase::EaseBasic::easeInBack(t, 2.5);
					FontSize = fontsize * (1.0f - easeT);
					charAlpha = static_cast<int>(255.0f * (1.0f - easeT));
				}
				else {
					FontSize = 0.0f;
					charAlpha = 0;
				}
			}

			// 绘制
			float t = len > 1 ? i / float(len - 1) : 0.0f;
			Color c = GetGradientColor(colorList, t);
			c.a = static_cast<uint8_t>(std::clamp(charAlpha, 0, 255));
			XText::SetFontConfig(c, FontSize);

			int tempY = y;
			unsigned char first = static_cast<unsigned char>(chars[i][0]);

			if (first > 0x7F && chars[i] != "，" && chars[i] != "。") {
				XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
			}
			else {
				XText::SetFontAdjust(ADJUST_CENTER, ADJUST_BOTTOM);
				tempY += fontsize / 3;
			}

			XText::Xyprintf(drawX, tempY, chars[i], window);
			drawX += charWidths[i];
		}
	}
};

static void Play()
{
	PlaySoundW(TEXT("Start.wav"), NULL, SND_FILENAME | SND_SYNC);
}

int RandomInt(int min, int max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dist(min, max);
	return dist(gen);
}
int ChangeInt(int v, int speed, int Min, int Max)
{
	int vv = v + speed;
	vv = max(Min, vv);
	vv = min(Max, vv);

	return vv;
}

void Hello(RenWin& window)
{
	bool isExit = false;
	int backAlpha = 0;

	int frame = 999;
	int totalframe = 0;
	int SleepFrame = 0;

	const string TextList[13] = {
		"AI赋能，高效板书。",
		"快速，便捷，轻量，极致",
		"Hello，MiuBarrd " + VER + " 。",
		"海上升明月，天涯共此时。",
		"MiuBarrd " + VER + " 准备好啦。",
		"世界那么大，很高兴遇见你。",
		"已更新至" + VER + "。",
		"准备，开！！！",
		"初见如旧雨，重逢似晨星。",
		"倾盖如故，白首如新。",
		"MiuBarrd准备就绪。",
		"适在AI，MiuBarrd已就绪",
		"潮平连旧渡，星动识归人。"
	};

	//初始化背景
	IMAGE Back[3];

	for (int i = 0; i < 3; i++)
	{
		XImage::NewImage(Back[i], ImgPath + L"Update\\" + to_wstring(i) + L".dll");
	}

	struct BackA
	{
		int x, y;
		int tag;
		int size;
		int frame;
		int totalframe;
		Color color;
	};

	Color TextColorNow = Color::Black;

	window.requestFocus();

	vector < Color> ColorNow;
	BackA BackNow[15];

	for (int i = 0; i < 15; i++)
	{
		BackNow[i].totalframe = RandomInt(600, 1500);
		BackNow[i].frame = RandomInt(0, BackNow[i].totalframe);
		BackNow[i].tag = RandomInt(0, 2);

		float C = RandomInt(-7, 7) / 20.0;
		if (C > 0) BackNow[i].color = XColor::LightColor(User::MainColor, C);
		else BackNow[i].color = XColor::DarkColor(User::MainColor, -C);

		BackNow[i].x = RandomInt(ScreenSize.x / 5, ScreenSize.x * 4 / 5);
		BackNow[i].y = RandomInt(-ScreenSize.y / 5, ScreenSize.y * 4 / 5);
		BackNow[i].size = RandomInt(100, ScreenSize.x / 5);
	}

	string text;
	int textnum;

	int ColorNum = 4;
	ColorNow.clear();
	for (int i = 0; i < ColorNum; i++)
	{
		Color candidate = XColor::LightColor(User::MainColor, RandomInt(0, 3) / 4.0);

		ColorNow.push_back(candidate);
	}

	backAlpha = 255;

	textnum = RandomInt(0, 12);
	text = TextList[textnum];
	frame = 0;
	totalframe = PutTextSay::Utf8Split(text).size() * 8;

	XWindow::SetBackGroundColor(Color(30, 35, 50));

	while (!XMsg::IsClose(window))
	{
		if (isExit && backAlpha >= 255) break;
		else if (backAlpha > 0)
		{
			backAlpha -= 5;
		}

		if (window.hasFocus()) XSystem::Taskbar::SetTaskBarVisible(false);
		else XSystem::Taskbar::SetTaskBarVisible(true);

		XWindow::DelayFps(window,60);

		for (int i = 0; i < 15; i++)
		{
			if (BackNow[i].frame <= BackNow[i].totalframe)
			{
				// 计算当前进度（0~1）
				float progress = BackNow[i].frame / float(BackNow[i].totalframe);
				float size = 0.0f;

				// 对称缩放：前半段放大，后半段缩小
				if (progress < 0.5f) {
					// 0 ~ 0.5: 0 -> max
					size = BackNow[i].size * (progress / 0.5f) / 100.0;
					Back[BackNow[i].tag].color.a = static_cast<int>(255 * (progress / 0.5f)); // 前半段逐渐变亮
				}
				else {
					// 0.5 ~ 1: max -> 0
					size = BackNow[i].size * ((1.0f - progress) / 0.5f) / 100.0;
					Back[BackNow[i].tag].color.a = static_cast<int>(255 * ((1.0f - progress) / 0.5f)); // 后半段逐渐变淡
				}

				// 保证size非负
				if (size < 0) size = 0;

				// 你可以根据需要调整缩放比例（如/100.0f），这里假设BackNow[i].size为像素单位
				int a = Back[BackNow[i].tag].color.a;
				Back[BackNow[i].tag].color = BackNow[i].color;
				Back[BackNow[i].tag].color.a = a;
				XImage::PutScaleImage(
					Back[BackNow[i].tag],
					BackNow[i].x, BackNow[i].y,
					size, size, window,
					0.5f, 0.5f
				);

				BackNow[i].frame++;
			}
			else
			{
				// 重新生成参数
				BackNow[i].frame = 0;
				BackNow[i].totalframe = RandomInt(600, 1500);
				BackNow[i].tag = RandomInt(0, 2);

				float C = RandomInt(-7, 7) / 20.0;
				if (C > 0) BackNow[i].color = XColor::LightColor(User::MainColor, C);
				else BackNow[i].color = XColor::DarkColor(User::MainColor, -C);

				BackNow[i].x = RandomInt(ScreenSize.x / 5, ScreenSize.x * 4 / 5);
				BackNow[i].y = RandomInt(-ScreenSize.y / 5, ScreenSize.y * 4 / 5);
				BackNow[i].size = RandomInt(100, ScreenSize.x / 5);
			}
		}

		XGraph::SetFillColor(Color(0, 0, 0, 100));
		XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, WindowSize.x, WindowSize.y,window);

		int w = WindowSize.x, h = WindowSize.y;

		if (XMsg::MouseMsg::IsMouseDown(MouseLeft))
		{
			isExit = true;
		}

		if (frame > totalframe && !down) // 展开动画结束，准备进入收回动画
		{
			SleepFrame = 15; // 展示停留时间
			down = true;
		}
		else if (frame < 0 && down) // 收回动画结束，准备进入下一轮
		{
			if (!isExit)
			{
				SleepFrame = 0; // 收回后停留时间
				down = false;

				// 切换新内容
				int ColorNum = 4;
				ColorNow.clear();
				while (ColorNow.size() < ColorNum) {
					Color candidate = XColor::LightColor(User::MainColor, RandomInt(0, 3) / 10.0);
					bool exists = false;
					for (const auto& c : ColorNow) {
						if (c == candidate) {
							exists = true;
							break;
						}
					}
					if (!exists) {
						ColorNow.push_back(candidate);
					}
				}
				int temp = -1;

				while (1)
				{
					temp = RandomInt(0, 12);
					if (textnum != temp) break;
				}

				text = TextList[temp];
				textnum = temp;

				frame = 0;
				totalframe = PutTextSay::Utf8Split(text).size() * 8;
			}
		}
		else if (SleepFrame > 0)
		{
			SleepFrame--;
		}
		else
		{
			// 动画进行中
			if (!down)
			{
				if (frame <= totalframe)
					frame++;
			}
			else
			{
				if (frame >= 0)
					frame -= 2; // 回收速度加快
			}
		}


		PutTextSay::Draw(
			w / 2,
			h / 2,
			text, { ColorNow }, frame, totalframe, 40, ScreenSize.x / 40, window);

		XText::SetFontColor(User::MainColor);
		XText::SetFontSize(25 * ScreenScale);
		XText::SetFontAdjust(ADJUST_CENTER, ADJUST_TOP);
		XText::Xyprintf(ScreenSize.x / 2, ScreenSize.y / 2 + 300, "点击任意区域继续", window);

		if (isExit)
		{
			backAlpha = ChangeInt(backAlpha, 10, 0, 255);
			XGraph::SetFillColor(Color(30, 30, 30, backAlpha));
			XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, w, h, window);
			SleepFrame = 0;
		}
		else
		{
			backAlpha = ChangeInt(backAlpha, -10, 0, 255);
			if (backAlpha > 0)
			{
				XGraph::SetFillColor(Color(30, 30, 30, backAlpha));
				XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, w, h, window);
			}
		}
	}

	XMsg::ResetCloseMsg();

	XWindow::SetBackGroundColor(Color(30, 30, 30));
}

#pragma endregion

void ShowUpdate(RenWin& window)
{
	Hello(window);
}

bool Update::CheckUpdate(RenWin& window)
{
	ifstream Check("Update.ini");
	if (Check.is_open())
	{
		Check >> ver;
		Check.close();

		if(ver != VER)
		{
			ShowUpdate(window);

			RightMessage::ShowMessage(L"MiuBarrd已成功更新至" + XString::Convert::utf8_to_wstring(VER), L"更新完成", RightMessageType_SUCCESS, true);

			/*ofstream Up("Update.ini");
			if (Up.is_open())
			{
				Up << VER;
				Up.close();
			}*/

			return true;
		}
	}
	else
	{
		Check.close();

		ShowUpdate(window);

		RightMessage::ShowMessage(L"MiuBarrd已成功更新至" + XString::Convert::utf8_to_wstring(VER), L"更新完成", RightMessageType_SUCCESS, true);

		/*ofstream Up("Update.ini");
		if (Up.is_open())
		{
			Up << VER;
			Up.close();
		}*/

		return false;
	}

	return false;
}