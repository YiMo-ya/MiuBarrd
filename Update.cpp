#include "Update.h"
#include "Shared.h"
#include "User.h"
#include "RightMessage.h"
#include "Video.h"
#include <random>
#include <thread>

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

//绘制问好文字
void DrawHelloText(RenWin& window,bool& isExit)
{

	static int frame = 999;
	static int totalframe = 0;
	static int SleepFrame = 0;

	static const string TextList[13] = {
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
	static Color TextColorNow = Color::Black;

	static vector < Color> ColorNow;

	static string text;
	static int textnum;

	static bool init = false;
	if(!init)
	{
		static int ColorNum = 4;
		ColorNow.clear();
		for (int i = 0; i < ColorNum; i++)
		{
			Color candidate = XColor::LightColor(User::MainColor, RandomInt(0, 3) / 4.0);

			ColorNow.push_back(candidate);
		}

		textnum = RandomInt(0, 12);
		text = TextList[textnum];
		frame = 0;
		totalframe = PutTextSay::Utf8Split(text).size() * 8;

		init = true;
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
		WindowSize.x / 2,
		WindowSize.y / 2,
		text, { ColorNow }, frame, totalframe, 40, ScreenSize.x / 40, window);

	if(isExit) SleepFrame = 0;
}

vector<IMAGE> video;




//背景视频
#pragma region MyRegion

bool LoadVideoFinish = false;
int VideoBackAlpha = 0;
void PerLoad()
{
	VideoLoader::LoadVideo(video, filesystem::current_path().wstring() + L"\\Update.dll");
	LoadVideoFinish = true;
	VideoBackAlpha = 255;
}

const int PLAY_VIDEO_FRAME_SLEEP = 3;
//绘制视频
void DrawVideo(RenWin& window)
{
	if (!LoadVideoFinish) return;

	static EV Scale;
	static bool init = false;
	if (!init)
	{
		Scale.SetAnimationStartValue(100);
		Scale.SetAnimation(115, 240);

		init = true;
	}

	static int index = 0;

	static int sleep = PLAY_VIDEO_FRAME_SLEEP;

	if (video.empty()) return;

	static float WScale = WindowSize.x / (float)video[0].w, HScale = WindowSize.y / (float)video[0].h;

	Scale.UpdateAnimation(XEase::EaseBasic::easeOut, 5);

	static int x = WindowSize.x / 2, y = WindowSize.y / 2;
	XImage::PutScaleImage(video[index], x,y, WScale * Scale.value / 100.0, HScale * Scale.value / 100.0, window,0.5,0.5);

	if (sleep == 0)
	{
		index += 1;
		if (index > video.size() - 1) index = 0;

		sleep = PLAY_VIDEO_FRAME_SLEEP;
	}
	else
	{
		sleep -= 1;
	}

	if (VideoBackAlpha > 0)
	{
		VideoBackAlpha = ChangeInt(VideoBackAlpha, -3, 0, 255);
		XGraph::SetFillColor(Color(30, 30, 30, VideoBackAlpha));
		XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, WindowSize.x, WindowSize.y, window);
	}
}

#pragma endregion


void Hello(RenWin& window)
{
	bool isExit = false;
	int backAlpha = 0;
	
	window.requestFocus();

	thread LoadVideo(PerLoad);
	LoadVideo.detach();

	backAlpha = 255;

	XWindow::SetBackGroundColor(Color(30, 30, 30));

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

		if (XMsg::MouseMsg::IsMouseDown(MouseLeft))
		{
			isExit = true;
		}

		DrawVideo(window);

		DrawHelloText(window,isExit);

		XText::SetFontColor(User::MainColor);
		XText::SetFontSize(25 * ScreenScale);
		XText::SetFontAdjust(ADJUST_CENTER, ADJUST_TOP);
		XText::Xyprintf(ScreenSize.x / 2, ScreenSize.y / 2 + 300, "点击任意区域继续", window);

		if (isExit)
		{
			backAlpha = ChangeInt(backAlpha, 20, 0, 255);
			XGraph::SetFillColor(Color(30, 30, 30, backAlpha));
			XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, WindowSize.x, WindowSize.y, window);
		}
		else
		{
			backAlpha = ChangeInt(backAlpha, -3, 0, 255);
			if (backAlpha > 0)
			{
				XGraph::SetFillColor(Color(30, 30, 30, backAlpha));
				XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, WindowSize.x, WindowSize.y, window);
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