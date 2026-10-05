#include "Update.h"
#include "Shared.h"
#include "User.h"
#include "RightMessage.h"
#include "Video.h"
#include <random>
#include <thread>

NOXS; NOSTD;

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
		"快速，便捷，轻量，极致，硬核",
		"Hello，MiuBarrd " + VER + " 。",
		"海上升明月，天涯共此时。",
		"MiuBarrd " + VER + " 准备好啦。",
		"世界那么大，很高兴遇见你。",
		"已更新至" + VER + "。",
		"准备，开！！！",
		"初见如旧雨，重逢似晨星。",
		"倾盖如故，白首如新。",
		"MiuBarrd准备就绪。",
		"庆祝MiuBarrd一周岁啦！！！",
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

vector<IMAGE> UpdateImgs;

//背景视频
#pragma region MyRegion

bool CanUpdateExit = false;

const int PLAY_FRAME = 240;
int PlayAlpha = 255;
int PlayClock = 0;
//绘制图片
void DrawVideo(RenWin& window)
{
	CanUpdateExit = false;
	static EV Scale;
	static bool init = false;
	if (!init)
	{
		Scale.SetAnimationStartValue(70);
		Scale.SetAnimation(60, 240);

		vector<Path> imgs = XFile::ListFiles(filesystem::current_path().wstring() + L"\\Update");

		for (int i = 0; i < imgs.size(); i++)
		{
			UpdateImgs.emplace_back();
			XImage::NewImage(UpdateImgs[UpdateImgs.size() - 1], imgs[i].wstring());
			XImageEx::ImageChange::Scale(
				UpdateImgs[UpdateImgs.size() - 1],
				WindowSize.x / (float)UpdateImgs[UpdateImgs.size() - 1].w,
				WindowSize.y / (float)UpdateImgs[UpdateImgs.size() - 1].h);
		}

		init = true;
	}

	static int index = 0;

	static int sleep = PLAY_FRAME;

	if (UpdateImgs.empty()) return;

	static float WScale = WindowSize.x / (float)UpdateImgs[0].w, HScale = WindowSize.y / (float)UpdateImgs[0].h;
	static int x = WindowSize.x / 2, y = WindowSize.y / 3;
	static int r = WindowSize.x / 200, lw = WindowSize.x / 500;

	Scale.UpdateAnimation(XEase::EaseBasic::easeOut, 5);

	XGraph::LineShape::SetLineWidth(lw);
	XGraph::SetColor(User::MainColor);
	int w = UpdateImgs[0].w * Scale.value / 96.0, h = UpdateImgs[0].h * Scale.value / 96.0;
	XGraph::RectangleShape::RoundRect(x - w / 2,y - h / 2,w,h ,r,window);

	UpdateImgs[index].color.a = 255 - PlayAlpha;
	XImage::PutScaleImage(UpdateImgs[index], x,y, WScale * Scale.value / 100.0, HScale * Scale.value / 100.0, window,0.5,0.5);
	
	if (PlayClock < PLAY_FRAME)
	{
		PlayClock += 1;

		if (PlayAlpha - 15 > 0) PlayAlpha -= 15;
		else PlayAlpha = 0;
	}
	else
	{
		if (PlayAlpha + 15 < 255) PlayAlpha += 15;
		else
		{
			PlayAlpha = 255;
			PlayClock = 0;

			index += 1;
			if (index > UpdateImgs.size() - 1)
			{
				CanUpdateExit = true;
				index = 0;
			}
		}
	}
}

#pragma endregion

//表情包
#pragma region MyRegion

struct Qs
{
	IMAGE img;

	EV Size;
};
vector<Qs> Qss;

void DrawQs(RenWin& window)
{
	static int Clock = 120;
	static int index = -1;
	Clock += 1;

	static bool Init = false;
	if (!Init)
	{
		vector<Path> imgs = XFile::ListFiles(filesystem::current_path().wstring() + L"\\Update\\Animation");

		for (int i = 0; i < imgs.size(); i++)
		{
			Qss.emplace_back();
			XImage::NewImage(Qss[i].img, imgs[i].wstring());
		}

		Init = true;
	}

	static int y = WindowSize.y * 3.1 / 4;
	static int x = WindowSize.x * 0.76;

	if (Clock > 120)
	{
		index += 1;
		if (index > Qss.size() - 1) index = 0;

		if (index < 0) index = 0;
		Qss[index].Size.SetAnimation(100, 30);
		if(index == 0) Qss[Qss.size() - 1].Size.SetAnimation(0, 30);
		else Qss[index - 1].Size.SetAnimation(0, 30);

		Clock = 0;
	}

	for (int i = 0; i < Qss.size(); i++)
	{
		if(i != index)
		{
			if (Qss[i].Size.end > 0) Qss[i].Size.UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
			else Qss[i].Size.UpdateAnimation(XEase::EaseBasic::easeInBack, 2);
		}
		else
		{
			int LastIndex = index - 1;
			if (LastIndex < 0) LastIndex = Qss.size() - 1;
			if (Qss[LastIndex].Size.frame > 30)
			{
				if (Qss[i].Size.end > 0) Qss[i].Size.UpdateAnimation(XEase::EaseBasic::easeOutBack, 2);
				else Qss[i].Size.UpdateAnimation(XEase::EaseBasic::easeInBack, 2);
			}
		}

		static float Scale = WindowSize.x / 10 / 512.0;

		if(Qss[i].Size.value > 0) 
			XImage::PutScaleImage(Qss[i].img, x, y, Scale * Qss[i].Size.value / 100.0, Scale * Qss[i].Size.value / 100.0, window, 0.5, 0.5);
	}
}

#pragma endregion

EV Process;

void UpdateThread()
{
	::Sleep(1000);

	Process.SetAnimation(20, 30);

	::Sleep(5000);

	Process.SetAnimation(97, 30);

	::Sleep(2000);

	Process.SetAnimation(100, 30);

	while (!CanUpdateExit)
	{
		::Sleep(10);
	}

	Process.SetAnimation(110, 30);
}

void DrawMicaBackground(
	sf::RenderWindow& window,
	float speed,
	const sf::Color& baseColor
)
{
	static sf::Shader shader;
	static bool shaderLoaded = false;
	static sf::Clock clock;
	static sf::RectangleShape fullscreenQuad;

	static const std::string fragSrc = R"(
    uniform float u_time;
        uniform float u_speed;
        uniform vec2  u_resolution;
        uniform vec3  u_baseColor;

        void main()
        {
            vec2 uv = gl_FragCoord.xy / u_resolution.xy;
            float t = u_time * u_speed * 0.3;

            // 从 baseColor 派生两个辅助色
            vec3 colA = u_baseColor * 0.4;                    // 深
            vec3 colB = u_baseColor;                          // 主色
            vec3 colC = clamp(u_baseColor * 1.25 + 0.08, 0.0, 1.0); // 亮

            // 三个缓慢移动的渐变中心
            vec2 c1 = vec2(0.3, 0.4) + vec2(sin(t * 0.7) * 0.2, cos(t * 0.5) * 0.15);
            vec2 c2 = vec2(0.7, 0.6) + vec2(cos(t * 0.4) * 0.25, sin(t * 0.6) * 0.2);
            vec2 c3 = vec2(0.5, 0.8) + vec2(sin(t * 0.3) * 0.15, cos(t * 0.8) * 0.1);

            float d1 = 1.0 - smoothstep(0.0, 0.9, length(uv - c1));
            float d2 = 1.0 - smoothstep(0.0, 0.8, length(uv - c2));
            float d3 = 1.0 - smoothstep(0.0, 0.7, length(uv - c3));

            vec3 color = colA;
            color = mix(color, colB, d1 * 0.7);
            color = mix(color, colC, d2 * 0.5);
            color = mix(color, colB, d3 * 0.4);

            // 极轻暗角
            color *= 1.0 - 0.15 * length(uv - 0.5);

            gl_FragColor = vec4(color, 1.0);
        }
)";

	if (!shaderLoaded)
	{
		if (!shader.loadFromMemory(fragSrc, sf::Shader::Type::Fragment))
		{
			window.clear(baseColor);
			return;
		}
		shaderLoaded = true;
		fullscreenQuad.setSize(sf::Vector2f(1, 1));
	}

	sf::Vector2u winSize = window.getSize();
	fullscreenQuad.setSize(sf::Vector2f((float)winSize.x, (float)winSize.y));

	float elapsed = clock.getElapsedTime().asSeconds();

	sf::Glsl::Vec3 base(
		baseColor.r / 255.0f,
		baseColor.g / 255.0f,
		baseColor.b / 255.0f
	);

	shader.setUniform("u_time", elapsed);
	shader.setUniform("u_speed", speed);
	shader.setUniform("u_resolution", sf::Glsl::Vec2((float)winSize.x, (float)winSize.y));
	shader.setUniform("u_baseColor", base);

	sf::RenderStates states;
	states.shader = &shader;
	window.draw(fullscreenQuad, states);
}

EV UpdateTextY;
void Update(RenWin& window)
{
	UpdateTextY.SetAnimationStartValue(WindowSize.y * 3 / 4);
	UpdateTextY.SetAnimation(WindowSize.y * 2.2 / 3, 180);

	Process.SetAnimationStartValue(0);

	thread temp(UpdateThread);
	temp.detach();

	int backAlpha = 0;

	window.requestFocus();

	backAlpha = 255;

	XWindow::SetBackGroundColor(Color(30, 30, 30));

	bool isExit = false;

	while (!XMsg::IsClose(window))
	{
		if (isExit && backAlpha >= 255) break;
		else if (backAlpha > 0)
		{
			backAlpha -= 5;
		}

		if (window.hasFocus()) XSystem::Taskbar::SetTaskBarVisible(false);
		else XSystem::Taskbar::SetTaskBarVisible(true);

		XWindow::DelayFps(window, 60);

		DrawVideo(window);

		Process.UpdateAnimation(XEase::EaseBasic::easeOut, 4);
		UpdateTextY.UpdateAnimation(XEase::EaseBasic::easeOut, 4);
		XText::SetFontColor(User::MainColor);
		XText::SetFontSize(35 * ScreenScale);
		XText::SetFontAdjust(ADJUST_LEFT, ADJUST_TOP);
		if(Process.value < 100) XText::Xyprintf(WindowSize.x * 0.3, UpdateTextY.value, "正在完成自动更新  "+ to_string((int)Process.end) + "%", window);
		else XText::Xyprintf(WindowSize.x * 0.3, UpdateTextY.value, "正在检查文件依赖性，再等一下下。", window);

		//进度条
		static int y = WindowSize.y * 3.2 / 4;
		static int x1 = WindowSize.x * 0.2, x2 = WindowSize.x * 0.7;
		static int l = x2 - x1;
		static int lw = WindowSize.x / 700;
		if(Process.value < 100)
		{
			XGraph::LineShape::SetLineWidth(lw);
			XGraph::SetColor(Color(100, 100, 100));
			XGraph::LineShape::Line(x1, y, x2, y, window);
			XGraph::LineShape::SetLineWidth(lw * 5);
			XGraph::SetColor(User::MainColor);
			XGraph::LineShape::Line(x1, y, x1 + l * Process.value / 100.0, y, window);
		}
		else
		{
			static int lg = l * 0.3;

			static bool IsIn = false;

			static EV p;
			if(!p.IsAnimation())
			{
				p.SetAnimationStartValue(-lg);
				p.SetAnimation(l, 60);
				IsIn = !IsIn;
			}

			if (IsIn) p.UpdateAnimation(XEase::EaseBasic::easeInOut, 2);
			else p.UpdateAnimation(XEase::EaseBasic::easeOut, 4);

			int Start1 = p.value; 
			int Start2 = Start1 + lg;
			Start2 = max(0, min(x2 - x1, Start2));
			Start1 = max(0, min(x2 - x1, (int)p.value));

			XGraph::LineShape::SetLineWidth(lw);
			XGraph::SetColor(Color(100, 100, 100));
			XGraph::LineShape::Line(x1, y, x2, y, window);
			XGraph::LineShape::SetLineWidth(lw * 5);
			XGraph::SetColor(User::MainColor);
			XGraph::LineShape::Line(x1 + Start1, y,x1 + Start2, y, window);
		}

		DrawQs(window);

		if (Process.value > 100) isExit = true;

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
}

void Hello(RenWin& window)
{
	bool isExit = false;
	int backAlpha = 0;
	
	window.requestFocus();

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

		static Color BackColor = XColor::DarkColor(User::MainColor, 0.2);
		DrawMicaBackground(window, 7.0, BackColor);

		DrawHelloText(window,isExit);

		XText::SetFontColor(User::MainColor);
		XText::SetFontSize(25 * ScreenScale);
		XText::SetFontAdjust(ADJUST_CENTER, ADJUST_TOP);
		XText::Xyprintf(WindowSize.x / 2, WindowSize.y * 3 / 4, "点击任意区域继续", window);

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
	Update(window);
	Hello(window);
}

bool Update::CheckUpdate(RenWin& window)
{
	ifstream Check("Update.ini");
	if (Check.is_open())
	{
		string ver;
		Check >> ver;
		Check.close();

		if(ver != VER)
		{
			ShowUpdate(window);

			RightMessage::ShowMessage(L"MiuBarrd已更新至" + XString::Convert::utf8_to_wstring(VER), L"更新完成", RightMessageType_SUCCESS, true);

			ofstream Up("Update.ini");
			if (Up.is_open())
			{
				Up << VER;
				Up.close();
			}

			return true;
		}
	}
	else
	{
		Check.close();

		ShowUpdate(window);

		RightMessage::ShowMessage(L"MiuBarrd已更新至" + XString::Convert::utf8_to_wstring(VER), L"更新完成", RightMessageType_SUCCESS, true);

		ofstream Up("Update.ini");
		if (Up.is_open())
		{
			Up << VER;
			Up.close();
		}

		return false;
	}

	return false;
}