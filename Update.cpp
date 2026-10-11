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
		int fadeInFrame, int fontsize, RenderWindow& window,
		bool playing = true
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

		int startX = x - totalWidth / 2;
		int intervalFrame = max(1, (totalFrame - fadeInFrame) / (len + 2));

		// 字符绘制顺序：从右往左遍历，使最左侧（i 最大）的字符最先开始动画，
		// 从而“左边的字先出来、也先回收”
		for (int i = len - 1; i >= 0; --i) {
			int charAlpha = 255;
			float FontSize = (float)fontsize; // 默认正常字号

			// 用本次调用的 playing 参数判断方向，而非全局 down：
			// 新旧两条文本同时绘制时，全局 down 已为 true，
			// 会让正在展开的下一条误走滑出分支、方向反转
			if (playing) {
				// ---- 滑入：0 → fontsize ----
				// 展开与回收统一为“从左往右”推进：i 越小越靠左、越先触发，
				// 因此这里与回收分支一样使用 i 的自然序计算延时
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
				// 回收阶段 frame 递减、outFrame = totalFrame - frame 递增，
				// 要让字符自左向右依次收回，触发顺序需与滑入时相反，
				// 故此处用 i 的自然序（i 越小越靠左、越先触发）计算延时
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

			// 该字符在整行中的横坐标偏移：按字符索引累加前序字符宽度，
			// 保证即使遍历顺序相反，字符仍落在正确的位置上，字序不会倒置
			int charX = startX;
			for (int k = 0; k < i; ++k) charX += charWidths[k];

			XText::Xyprintf(charX, tempY, chars[i], window);
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

	// ---- 交叉过渡状态：上一条刚开始收缩时，下一条立刻同步展开 ----
	static bool showNext = false;       // 下一条是否正在展开（与上一条的收缩同时进行）
	static int nextFrame = 0;           // 下一条的展开进度帧
	static int nextTotalframe = 0;      // 下一条的总帧数
	static string nextText;             // 下一条文本内容
	static vector<Color> nextColorNow;  // 下一条文本的渐变色
	static int nextDelayFrame = 0;      // 下一条真正开始展开前的延迟帧数（等待这段时间再展开）

	// 本条展开结束后、真正开始收缩之前的停留帧数
	const int HOLD_FRAMES = 15;

	// 上一条开始收缩后，下一条额外等待的帧数。
	// 让当前文本先回收一部分、屏幕上留出空隙，下一条再展开，避免新旧文字过于拥挤
	const int NEXT_DELAY_FRAMES = 15;

	// 抽取一条与当前内容不同的文本，避免连续重复
	auto PickAnotherText = [&]() -> int {
		int temp = textnum;
		while (temp == textnum) temp = RandomInt(0, 12);
		return temp;
	};

	// 生成一组不重复的渐变色
	auto MakeColors = [&]() -> vector<Color> {
		vector<Color> res;
		while (res.size() < 4) {
			Color candidate = XColor::LightColor(User::MainColor, RandomInt(0, 3) / 10.0);
			bool exists = false;
			for (const auto& c : res) {
				if (c == candidate) { exists = true; break; }
			}
			if (!exists) res.push_back(candidate);
		}
		return res;
	};

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

	if (frame > totalframe && !down) // 本条文本已完全展开
	{
		// 停留阶段：本条文本完整显示，等待 HOLD_FRAMES 帧后再开始收缩
		if (SleepFrame > 0) SleepFrame--;
		else if (!showNext)
		{
			// 停留结束：本条开始收缩的同时，立刻启动下一条的展开，
			// 两条文本此后各自独立推进，让新旧在屏幕上有一段明显的重叠
			SleepFrame = 0;
			down = true;

			if (!isExit)
			{
				nextText = TextList[PickAnotherText()];
				nextColorNow = MakeColors();
				nextTotalframe = PutTextSay::Utf8Split(nextText).size() * 8;
				nextFrame = 0;
				nextDelayFrame = NEXT_DELAY_FRAMES; // 先进入延迟等待，下一条暂不展开
				showNext = true;
			}
		}
	}
	else if (showNext && nextFrame > nextTotalframe && down) // 下一条已完全展开
	{
		if (!isExit)
		{
			// 下一条接管为当前内容；此时上一条早已收缩归零，重叠自然结束
			text = nextText;
			ColorNow = nextColorNow;
			totalframe = nextTotalframe;
			frame = totalframe + 1; // 直接视作“已完全展开”，进入停留阶段

			// 同步 textnum，保证下一轮抽签的“避免重复”逻辑依然有效
			for (int i = 0; i < 13; i++)
			{
				if (TextList[i] == text) { textnum = i; break; }
			}

			SleepFrame = HOLD_FRAMES;
			down = false;
			showNext = false;
			nextFrame = 0;
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

	// 下一条的展开进度独立推进，与上一条的收缩互不干扰
	if (showNext)
	{
		// 先消耗延迟帧，延迟结束后下一条才开始真正展开
		if (nextDelayFrame > 0) nextDelayFrame--;
		else if (nextFrame <= nextTotalframe) nextFrame++;
	}

	// 当前文本按其自身状态播放：展开时用滑入，收缩时用滑出
	PutTextSay::Draw(
		WindowSize.x / 2,
		WindowSize.y / 2,
		text, { ColorNow }, frame, totalframe, 40, ScreenSize.x / 40, window, !down);

	// 上一条还没收回完时，叠加绘制正在展开的下一条，形成新旧交叉过渡
	if (showNext)
	{
		PutTextSay::Draw(
			WindowSize.x / 2,
			WindowSize.y / 2,
			nextText, { nextColorNow }, nextFrame, nextTotalframe, 40, ScreenSize.x / 40, window, true);
	}

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

            // 从 baseColor 派生出底色与高亮色，保证与主题色一致
            vec3 colDark = u_baseColor * 0.25;                          // 顶部/整体深色底
            vec3 colBase = u_baseColor;                                 // 主色
            vec3 colGlow = clamp(u_baseColor * 1.6 + 0.25, 0.0, 1.0);    // 底部发光亮色

            // 底部发光核心：光源被移到屏幕下边缘之外（uv.y < 0），
            // 因此屏幕内越靠近下边缘越亮，形成“光从屏幕下方透上来”的辉光。
            // 这里用 (1.0 + offset - uv.y) 让衰减曲线的峰值落在屏幕外，
            // pow 指数取 1.6（比原来更低）使亮区向上铺得更远，过渡更长更柔。
            const float glowOffset = 0.10;                    // 光源位于屏幕下方 0.10 个屏幕高度处
            float bottom = pow(max(1.0 + glowOffset - uv.y, 0.0) / (1.0 + glowOffset), 2.0);

            // ---- 流动感核心：几个圆形光斑沿底缘横向漂移 ----
            // 关键点：光源是「圆形」的径向光团（而非沿 x 拉伸的长条），
            // 因此亮度按到光斑圆心的二维距离衰减；每个光斑沿 x 轴移动，
            // 并把拖尾方向（运动后方）按同样的圆形距离渐隐，形成彗尾式动态模糊。
            // 用「非等比坐标」计算距离：x 方向尺度小、y 方向尺度大，
            // 使光斑在屏幕上呈现为竖直略扁的圆，而非一条横线。
            float streak = 0.0;

            // 单个圆形光斑：center 为圆心（随 t 沿 x 漂移），y 固定在底缘附近；
            // scaleX/scaleY 控制光斑的横向/纵向半径，决定它是「圆」还是「线」。
            // 采用二维距离的平滑衰减，得到真正的径向圆形辉光。
            // 三条光斑速度/相位/大小都不同，避免同步的机械感。

            // 光斑 1：主光斑，快速右移，拖尾较长
            // 注意 cy* 均为负值：把三个光斑的圆心全部放到屏幕下边缘之外，
            // 屏幕内只会看到它们向上扩散出来的辉光，而不是完整的光球本身。
            float cx1 = fract(0.15 + 0.10 * t);        // 用 fract 让圆心循环，避免移出视野
            float cy1 = -0.15;                          // 圆心在屏幕下方之外
            float rad1 = 0.6;                          // 扩大光斑半径（原 0.07）
            vec2  d1 = vec2(uv.x - cx1, (uv.y - cy1) * 0.70); // 纵向压缩比例调小 -> 辉光沿 y 扩散范围更大
            d1.x -= floor(d1.x + 0.5);                  // 只对 x 环绕到 [-0.5,0.5]，保证跨边界平滑
            float base1 = 1.0 - smoothstep(0.0, rad1, length(d1)); // 二维径向核心亮斑
            // 拖尾：在运动后方（这里取 d1.x>0 一侧）沿同样的径向距离渐隐，形成圆形慧尾
            float asym1 = mix(0.45, 1.0, 1.0 - smoothstep(0.0, rad1 * 2.4, max(d1.x, 0.0)));
            streak += 0.85 * base1 * asym1;

            // 光斑 2：次光斑，反向移动，速度较慢
            float cx2 = fract(0.62 - 0.13 * t);
            float cy2 = -0.15;                          // 更深地置于屏幕下方之外
            float rad2 = 0.4;                          // 扩大光斑半径（原 0.055）
            vec2  d2 = vec2(uv.x - cx2, (uv.y - cy2) * 0.70);
            d2.x -= floor(d2.x + 0.5);
            float base2 = 1.0 - smoothstep(0.0, rad2, length(d2));
            // 反向移动 -> 拖尾在另一侧
            float asym2 = mix(0.55, 1.0, 1.0 - smoothstep(0.0, rad2 * 2.0, max(-d2.x, 0.0)));
            streak += 0.60 * base2 * asym2;

            // 光斑 3：小高光点，慢速，最锐利
            float cx3 = fract(0.30 + 0.06 * t);
            float cy3 = -0.1;                          // 圆心在屏幕下方之外
            float rad3 = 0.3;                          // 扩大光斑半径（原 0.035）
            vec2  d3 = vec2(uv.x - cx3, (uv.y - cy3) * 0.70);
            d3.x -= floor(d3.x + 0.5);
            float base3 = 1.0 - smoothstep(0.0, rad3, length(d3));
            float asym3 = mix(0.25, 1.0, 1.0 - smoothstep(0.0, rad3 * 2.6, max(d3.x, 0.0)));
            streak += 0.45 * base3 * asym3;

            // 光斑沿线分布不均：让某些位置的亮斑被削弱，
            // 于是同一时刻只有部分区域有明显高光，其余区域接近无光，强化「漂移」的感觉。
            float mask = 0.35 + 0.65 * (0.5 + 0.5 * sin(t * 0.4 + uv.x * 1.5));
            streak *= mask;

            // clamp 防止三个光斑叠加处过曝
            streak = clamp(streak, 0.0, 1.0);

            // 亮斑只在靠近底缘处出现：把底部辉光的高度当作纵向包络，
            // 避免整片下半区都在发光。包络范围放宽（0.55 -> 0.85）
            // 以适应光源外移后更大的扩散区间。
            float band = smoothstep(0.0, 0.85, bottom);

            float glow = bottom * band * streak;

            vec3 color = mix(colDark, colBase, bottom * 0.9);        // 整体自下而上的主色过渡
            color = mix(color, colGlow, glow * 0.8);                 // 底部叠加发光高亮

            // 极轻暗角，保持与原有风格一致
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

	XWindow::SetBackGroundColor(User::BackColor);

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

	XWindow::SetBackGroundColor(User::BackColor);

	static int StartClock = 0;

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

		DrawMicaBackground(window, 4.5, User::MainColor);

		DrawHelloText(window,isExit);

		XText::SetFontColor(User::MainColor);
		static int FontSize = WindowSize.x / 45;
		XText::SetFontSize(FontSize);
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

	XWindow::SetBackGroundColor(User::BackColor);
}

#pragma endregion

void ShowUpdate(RenWin& window)
{
	//Update(window);
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