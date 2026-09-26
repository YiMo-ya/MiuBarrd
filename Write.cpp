#include "Write.h"
#include "Tool.h"
#include "Shared.h"
#include "Message.h"
#include "BottomMessage.h"
#include "RightMessage.h"
#include "User.h"
#include "Camera.h"
#include "ImageScan.h"
#include <stack>

NOXS; NOSTD;

//全局触控数量
int TouchNum = 0;

//辅助函数
#pragma region MyRegion

bool IsTouching()
{
	return ((XMsg::KeyMsg::Keystate(0x01) || XMsg::MouseMsg::IsMouseDown(MouseLeft)
		|| XMsg::KeyMsg::Keystate(0x02) || XMsg::MouseMsg::IsMouseDown(MouseRight)) && TouchNum < 2) || TouchNum > 0;
}

void Clamp(int& v, int MIN, int MAX)
{
	if (MIN > MAX) swap(MIN, MAX);

	v = max(MIN, v);
	v = min(MAX, v);
}

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

// 颜色选择窗口（公开实现，与 Tool.h 中的声明保持一致；
// 不可加 static，否则其他翻译单元（如 Set.cpp）无法链接到该符号）
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
		Message::ShowMessage(WrongMsg, L"MiuBarrd");

		return cancelColor;
	}

	RenderWindow ColorChooseWindow;
	XWindow::CreateGraphWindow(ColorChooseWindow, -1, -1, ScreenSize.x / 3, ScreenSize.y / 2, L"选择颜色", Style::Default);

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
		XWindow::DelayFps(ColorChooseWindow, 30);
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

	XWindow::SetBackGroundColor(Color(30, 30, 30));
	return ReturnColor;
}

#pragma endregion

#pragma endregion

//启用展台
bool WriteCamera::EnableWriteCamera = false;
bool WriteCamera::EnableAutoPhoto = false;

bool FlushWriteLayer = false;

bool FlushImageLayer = false;

//思维导图节点数据（extern 声明：定义见下方「思维导图」区域，
// 此处提前声明供 Write::Show 处理调色请求时访问）
extern vector<MindMap::Node> MindMapNodes;

//按编号查找节点下标，找不到返回 -1（前向声明，定义见思维导图区域）
static int MindMapIndexOf(int id);

//归位量
EV2 ResetPos;

//橡皮擦大小
int Write::EraseSize = 40;
const int EraseBasicSize = 40;
//是否擦除
bool IsErasing = false;

//页码
int Write::TotalPage = 0, Write::Page = 0;

//页面移动速度
Vector2i MoveSpeed;

//暂停书写时间
static int WriteStopTime = 0;

//最大储存块
const int MAX_MEMORY_BLOCK = 1500;

//页面数据
#pragma region MyRegion

struct WriteData
{
	Color color = Color::White;
	float x, y, x2, y2;

	float w = 1;

	//柳叶笔用
	int StartX = -1, StartY = -1;
	//荧光笔专用
	bool Light = false;

	bool TempLayer = false;
};

//临时书写数据
vector< WriteData > WriteDataTemp;

struct UnDoStack
{
	int Index;
	vector<WriteData> UnDoData;
};

struct ImageStruct
{
	wstring path;

	IMAGE img;
	Vector2i pos = { 0,0 };

	int rote = 0;

	int w, h;
};
struct PageDataS
{
	vector < vector < WriteData > > Data;
	stack < UnDoStack > UnDoData;

	vector< ImageStruct > Images;

	Vector2i CameraPos;
	int Scale = 100;

	bool ShowCamera = false;
};
vector< PageDataS > PageData = { PageDataS()};

//展台专用页面数据
vector< PageDataS > CameraPageData = { PageDataS() };

//获取当前正在使用的页面数据引用
PageDataS& GetCurPage()
{
	// 展台专用页使用独立的页面数据，避免与普通板书页互相干扰
	if (WriteCamera::EnableWriteCamera) return CameraPageData[Write::Page];
	else return PageData[Write::Page];
}

#pragma endregion

//SDF 线段抗锯齿
#pragma region MyRegion

//片元着色器：逐像素求"到最近线段的距离"，再用 fwidth 求解析 AA 带宽
static const char* kSDFLineFragment = R"(
uniform vec2  uSegs[512];     // 线段端点打包：偶数项=起点，奇数项=终点（画布像素）
uniform float uHalfW;         // 半线宽（像素）
uniform int   uCount;         // 有效线段数
uniform float uAlpha;         // 整体不透明度（荧光笔用）

// 点到线段的精确距离（含端点夹取），SDF 的核心
float sdSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a;
    vec2 ba = b - a;
    // 除零保护：退化为点时直接取点距
    float denom = dot(ba, ba);
    float h = denom > 1e-6 ? clamp(dot(pa, ba) / denom, 0.0, 1.0) : 0.0;
    return length(pa - ba * h);
}

void main()
{
    // 顶点把画布像素坐标写进 texCoord，这里直接取用
    vec2 p = gl_TexCoord[0].xy;

    float d = 1e9;
    for (int i = 0; i < 256; i++)
    {
        if (i >= uCount) break;
        d = min(d, sdSegment(p, uSegs[i * 2], uSegs[i * 2 + 1]));
    }

    // 有向距离 < 0 表示在笔画内部；用半像素带宽做解析 AA
    d -= uHalfW;
    float aa = max(fwidth(d) * 0.5, 1e-4);
    float alpha = 1.0 - smoothstep(-aa, aa, d);
    if (alpha <= 0.0) discard;

    gl_FragColor = vec4(gl_Color.rgb, gl_Color.a * uAlpha * alpha);
}
)";

/// <summary>
/// SDF 线段渲染器：把一批同色同宽线段以"单个 quad + 距离场"的方式绘制。
/// draw call 恒为 1 次，AA 质量与最终呈现分辨率无关，替代 RenderTexture 超采样。
/// </summary>
class SDFLineRenderer
{
public:
    /// <summary>单批可容纳的最大线段数（受 uniform 数组长度限制）。</summary>
    static const int kMaxSegs = 256;

    /// <summary>获取全局唯一实例。</summary>
    static SDFLineRenderer& Instance()
    {
        static SDFLineRenderer inst;
        return inst;
    }

    /// <summary>着色器是否可用；失败时调用方应回退到普通绘制。</summary>
    bool Ready()
    {
        if (m_tried) return m_ready;
        m_tried = true;

        if (!sf::Shader::isAvailable()) return false;

        m_ready = m_shader.loadFromMemory(kSDFLineFragment, sf::Shader::Type::Fragment);
        return m_ready;
    }

    /// <summary>
    /// 绘制一批线段。
    /// </summary>
    /// <param name="target">绘制目标（窗口或 RenderTexture）。</param>
    /// <param name="segs">端点数组，两两一组表示一条线段，单位为目标像素坐标。</param>
    /// <param name="segCount">线段条数，超过 kMaxSegs 时截断。</param>
    /// <param name="halfW">半线宽（像素）。</param>
    /// <param name="color">颜色，alpha 参与混合。</param>
    /// <param name="alpha">额外整体不透明度，荧光笔传 150/255，其余传 1.0。</param>
    void Draw(sf::RenderTarget& target,
              const sf::Glsl::Vec2* segs,
              int segCount,
              float halfW,
              const sf::Color& color,
              float alpha)
    {
        if (!Ready() || segCount <= 0) return;
        // 截断而非报错，避免 uniform 越界读取
        if (segCount > kMaxSegs) segCount = kMaxSegs;

        sf::Vector2f size(target.getSize());
        if (size.x <= 0.f || size.y <= 0.f) return;

        // 数组 uniform 必须走 setUniformArray 重载（setUniform 不接受指针 + 长度）
        m_shader.setUniformArray("uSegs", segs, (std::size_t)kMaxSegs * 2);
        m_shader.setUniform("uHalfW", halfW);
        m_shader.setUniform("uCount", segCount);
        m_shader.setUniform("uAlpha", alpha);

        // 关键性能点：quad 只覆盖这批线段的包围盒，而不是整块画布。
        // 片元深度与"覆盖面积 × 线段数"成正比，全屏 quad 在 4K 下会让
        // 单个像素跑满 uCount 次距离计算，写第一条线就卡顿。
        // 手写包围盒：避免 sf::Rect 在 SFML 3 中的构造/成员差异，
        // 同时绕开 Windows min/max 宏对 position/size 等成员名的干扰
        float minX = segs[0].x, maxX = segs[0].x;
        float minY = segs[0].y, maxY = segs[0].y;
        for (int i = 0; i < segCount; ++i)
        {
            const float x = segs[i * 2].x, y = segs[i * 2].y;
            const float x2 = segs[i * 2 + 1].x, y2 = segs[i * 2 + 1].y;

            minX = (minX < x) ? minX : x;
            minX = (minX < x2) ? minX : x2;
            maxX = (maxX > x) ? maxX : x;
            maxX = (maxX > x2) ? maxX : x2;
            minY = (minY < y) ? minY : y;
            minY = (minY < y2) ? minY : y2;
            maxY = (maxY > y) ? maxY : y;
            maxY = (maxY > y2) ? maxY : y2;
        }

        // 外扩半线宽 + 1px AA 余量，保证边缘与端点不被裁掉
        const float pad = halfW + 1.f;
        float left = minX - pad;
        float top = minY - pad;
        float right = maxX + pad;
        float bottom = maxY + pad;

        // 裁剪到目标画布范围内，避免超大包围盒造成无谓填充
        left = (left > 0.f) ? left : 0.f;
        top = (top > 0.f) ? top : 0.f;
        right = (right < size.x) ? right : size.x;
        bottom = (bottom < size.y) ? bottom : size.y;
        if (right <= left || bottom <= top) return;

        sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
        quad[0] = sf::Vertex(sf::Vector2f(left, top), color);
        quad[1] = sf::Vertex(sf::Vector2f(right, top), color);
        quad[2] = sf::Vertex(sf::Vector2f(left, bottom), color);
        quad[3] = sf::Vertex(sf::Vector2f(right, bottom), color);
        // 顶点携带画布像素坐标，片元据此重建像素位置（与包围盒裁剪无关）
        for (int i = 0; i < 4; ++i)
            quad[i].texCoords = quad[i].position;

        sf::RenderStates states;
        states.shader = &m_shader;
        target.draw(quad, states);
    }

private:
    SDFLineRenderer() = default;
    SDFLineRenderer(const SDFLineRenderer&) = delete;
    SDFLineRenderer& operator=(const SDFLineRenderer&) = delete;

    sf::Shader m_shader;    ///< SDF 线段片元着色器
    bool m_ready = false;   ///< 编译是否成功
    bool m_tried = false;   ///< 是否已尝试编译，避免重复失败的编译开销
};

//SDF 线段打包缓冲：把 WriteData 转成着色器需要的端点数组
vector<sf::Glsl::Vec2> g_SDFSegs;
sf::Color g_SDFColor = sf::Color::White;
float g_SDFHalfW = 1.f;

/// <summary>
/// 清空当前 SDF 批次，准备收集一组同色同宽的线段。
/// </summary>
static void SDFBatchBegin(const sf::Color& color, float halfW, float alpha)
{
    g_SDFColor = color;
    // alpha 单独缩放：荧光笔整体半透明由这里的 alpha 控制，颜色本身的 alpha 仍参与混合
    g_SDFColor.a = static_cast<std::uint8_t>(color.a * alpha);
    g_SDFHalfW = halfW;
    g_SDFSegs.clear();
    g_SDFSegs.reserve(SDFLineRenderer::kMaxSegs * 2);
}

/// <summary>
/// 向当前 SDF 批次加入一条线段；批次满时自动冲刷一次。
/// </summary>
static void SDFBatchPush(sf::RenderTarget& target, float x1, float y1, float x2, float y2)
{
    g_SDFSegs.push_back(sf::Glsl::Vec2(x1, y1));
    g_SDFSegs.push_back(sf::Glsl::Vec2(x2, y2));

    if ((int)g_SDFSegs.size() >= SDFLineRenderer::kMaxSegs * 2)
    {
        SDFLineRenderer::Instance().Draw(
            target, g_SDFSegs.data(), (int)g_SDFSegs.size() / 2, g_SDFHalfW, g_SDFColor, 1.f);
        g_SDFSegs.clear();
    }
}

/// <summary>
/// 冲刷当前 SDF 批次，把已收集的线段一次性提交绘制。
/// </summary>
static void SDFBatchFlush(sf::RenderTarget& target)
{
    if (g_SDFSegs.empty()) return;

    SDFLineRenderer::Instance().Draw(
        target, g_SDFSegs.data(), (int)g_SDFSegs.size() / 2, g_SDFHalfW, g_SDFColor, 1.f);
    g_SDFSegs.clear();
}

#pragma endregion

//临时层
RenderTexture WriteTempLayer;
//荧光层
RenderTexture LightLayer;
//书写层
RenderTexture WriteLayer;
//图片层
RenderTexture ImageLayer;
//思维导图层
RenderTexture MindMapLayer;

//视觉特效
#pragma region MyRegion

struct Per
{
	int x, y, x2, y2;
	Color color;
	int a;
};
vector<Per> pers;

//绘制特效
void DrawPer(RenWin& window)
{
	if (pers.empty()) return;

	// 倒序遍历：需要删除元素时直接 pop_back，避免 erase(begin+i) 造成的 O(n) 搬移
	for (int i = (int)pers.size() - 1; i >= 0; i--)
	{
		auto& per = pers[i];

		per.a -= 10;
		if (per.a <= 0)
		{
			// 尾部元素直接弹出；中间元素与末尾交换后弹出，保持 O(1) 删除
			pers[i] = pers.back();
			pers.pop_back();
			continue;
		}

		per.color.a = per.a;
		XGraph::SetColor(per.color);
		XGraph::LineShape::SetLineWidth(Tool::PenSize);
		XGraph::LineShape::Line(per.x, per.y, per.x2, per.y2, window);
	}
}

//添加特效
void AddPer(int x, int y, int x2, int y2, Color color)
{
	pers.push_back({ x,y,x2,y2,color,255 });
}

#pragma endregion

//思维导图
#pragma region MyRegion

//思维导图节点数据
vector<MindMap::Node> MindMapNodes;
//当前选中节点编号（决定新子节点挂载到哪个父节点）
int MindMapSelect = -1;
//节点编号计数器
int MindMapNextId = 0;
//思维导图是否已被用户激活（只有点击插件添加后才显示，避免一进页面就自动出现）
bool MindMapActived = false;
//本帧触控是否落在思维导图的 + 按钮上：供书写系统判断是否跳过落笔，避免点击按钮时穿透书写
bool MindMapBtnHit = false;
//本次触控周期内是否曾命中过按钮（粘滞标记）：从按下到抬起期间持续为 true，
//防止按住按钮时坐标轻微漂移导致中途漏屏蔽、进而发生书写穿透。
bool MindMapWasBtnHit = false;
//上一次触控状态：用于在 Update 内部识别「按下的一瞬间」
bool MindMapLastTouch = false;
//请求调色的节点编号：-1 表示无请求；由外部（Tool 层）读取后弹出调色窗口并复位
int MindMapColorRequest = -1;
//「按钮按住中」标记：从命中按钮的那一帧持续到抬手，用于全程抑制底线自动延展。
// 之所以不能用「当帧命中」判断：抬手那一帧命中的是「无」，但 WriteSub 仍可能提交
// 最后一次残留笔迹，从而把底线撑长；用本标记可覆盖到抬起当帧。
bool MindMapBtnHold = false;

//底线动态拓展的动画时长（帧），参照对齐线的缓动表现
static const int MINDMAP_LINE_ANIM = 30;

//内容底部到底线的间隙（父节与底线的美感空隙）
static int MindMapLineGap()
{
	static int g = max(4, WindowSize.y / 90);
	return g;
}
//新节点创建时的初始纵向落点偏移（父项底线 -> 新子项）。
// 说明：这只是「创建瞬间的初值」，真正生效的位置由 MindMapLayoutNode 统一重排。
// 因此该值不再承担「父子呼吸空隙」的职责（那是 MindMapSiblingGap 的活儿），
// 取小值即可，避免初值过大导致首帧位置跳变明显。
static int MindMapRowGap()
{
	static int g = max(6, WindowSize.y / 40);
	return g;
}

//同级兄弟节点之间的最小纵向间距（行间距），同时也决定「父项与子项」的行距。
//
// 关键：这里是整棵树唯一的纵向节奏来源。
// MindMapLayoutNode 中，子项的 minY 只取 parentBottom + MindMapSiblingGap()，
// 因此本值每缩小一点，层与层之间的空隙就整体收紧一点；
// 之前取 WindowSize.y / 16（偏大），导致「子项本身还有子项」时，
// 每层都叠加一次该间距，视觉上纵向空隙迅速累积、显得非常空旷。
// 现收紧到 WindowSize.y / 26，并保留「底线上方内容区」不被压到的下界，
// 使父子、兄弟两层关系都保持紧凑但不拥挤。
static int MindMapSiblingGap()
{
	static int g = max(6, WindowSize.y / 26);
	return g;
}
//连接线「子项一侧」的断开缝隙（世界单位）。
static int MindMapLinkDetach()
{
	static int d = max(10, WindowSize.y / 30);
	return d;
}

//连接线「父项一侧」的断开缝隙（世界单位）。
static int MindMapLinkDetachParent()
{
	static int d = max(10, WindowSize.y / 30);
	return d;
}

//单个节点内容区的默认高度
static int MindMapContentH()
{
	static int h = WindowSize.y / 10;
	return h;
}
//连接线起点相对「父项最后一个按钮（调色按钮）」右缘的额外呼吸空隙（世界单位）。
// 存在的意义：连线必须从三个按钮全部之后起笔，才不会被按钮压在下面；
// 同时保留一小段空隙，避免连线紧贴按钮边缘。
static int MindMapLinkStartPad()
{
	static int p = max(6, WindowSize.y / 60);
	return p;
}

//按钮尺寸与间距的前向声明：
// MindMapBtnZoneW 需要用到它们，但它们的定义在下方（便于按「由外到内」的顺序阅读）。
static int MindMapBtnR();
static int MindMapBtnSep();
static int MindMapDelBtnSep();
static int MindMapColorBtnSep();

//节点「按钮区」占用的总宽度（世界单位）：+ / 删除(-) / 调色 三个圆点按钮，
// 每个直径 2r，相邻圆心间距 r*2 + 对应间距。
// 关键作用：连接线的起点与子节点的横向落点都以「按钮区右缘」为基准，
// 这样按钮区变宽时连接线与子项会自动整体右移，不会被按钮覆盖。
static int MindMapBtnZoneW()
{
	int r = MindMapBtnR();
	// 底线尾 -> 第一个按钮圆心（sep + r），按钮区内部两段间距，最后一颗按钮半径 r
	return MindMapBtnSep() + r + (r * 2 + MindMapDelBtnSep())
		+ (r * 2 + MindMapColorBtnSep()) + r;
}
//底线右端 + 圆形按钮半径
static int MindMapBtnR()
{
	// 在原尺寸基础上缩小 30%（0.7 倍），使按钮更精致、不抢底线视觉重心
	static int r = max(7, (int)(WindowSize.y / 58 * 0.7));
	return r;
}
//「删除(-)」按钮圆心与「调色」按钮圆心之间的横向间距
static int MindMapDelBtnSep()
{
	static int s = max(8, WindowSize.y / 70);
	return s;
}
//+ 按钮圆心与底线右端之间的间距（按钮与底线分离）
static int MindMapBtnSep()
{
	static int s = max(8, WindowSize.y / 70);
	return s;
}
//「调色」按钮圆心与 + 按钮圆心之间的横向间距
static int MindMapColorBtnSep()
{
	static int s = max(8, WindowSize.y / 70);
	return s;
}
//底线在手写内容之后额外保留的收尾长度（让底线比内容略长）
static int MindMapLineTail()
{
	static int t = WindowSize.x / 30;
	return t;
}
//横向层级缩进
static int MindMapIndent()
{
	static int i = WindowSize.x / 10;
	return i;
}
//底线相对内容左右额外延长量（呼吸空间，避免贴边）
static int MindMapLinePad()
{
	static int p = WindowSize.x / 60;
	return p;
}

//按编号查找节点下标，找不到返回 -1
static int MindMapIndexOf(int id)
{
	if (id < 0) return -1;
	for (int i = 0; i < (int)MindMapNodes.size(); i++)
		if (MindMapNodes[i].id == id) return i;
	return -1;
}

//递归收集某节点整棵子树（含自身）的编号
static void MindMapCollectSubtree(int id, vector<int>& out)
{
	int idx = MindMapIndexOf(id);
	if (idx < 0) return;

	out.push_back(id);
	for (int c : MindMapNodes[idx].children)
		MindMapCollectSubtree(c, out);
}

//节点底线的初始长度（保证 + 按钮与底线始终可见、永不消逝）
static int MindMapLineInitW()
{
	static int w = WindowSize.x / 10;
	return w;
}

//推进所有节点底线的缓动动画（lineEase 从当前值逼近目标长度 lineW）。
// 单独抽出成函数：Draw 与 Update 都按「激活态」门控，
// 未激活时（首次激活前 / Clear 之后）若完全不跑缓动，
// 底线会停在半途不再收敛，再次激活绘制时从残值突然续跳。
// 因此无论是否激活，每帧都调用本函数保证动画连续。
static void MindMapUpdateLineEase()
{
	for (auto& n : MindMapNodes)
	{
		// 目标长度与缓动终点不一致时，说明长度需要变化，启动一次缓动
		if ((int)n.lineEase.end != n.lineW)
		{
			n.lineEase.SetAnimation(n.lineW, MINDMAP_LINE_ANIM);
		}

		n.lineEase.UpdateAnimation(XEase::EaseBasic::easeOut, 4);
	}
}

//创建新节点并登记到数组；返回新节点编号
static int MindMapCreateNode(int parent, int x, int y, Color color)
{
	MindMap::Node n;
	n.id = MindMapNextId++;
	n.parent = parent;
	n.x = x;
	n.y = y;
	// 底线给一个初始长度：即使没有手写内容也始终显示底线和 + 按钮。
	// 新节点直接以初始长度作为缓动起点，避免出现从 0 拉到初始值的异常动画。
	n.lineW = MindMapLineInitW();
	n.lineH = MindMapContentH();
	n.color = color;
	n.lineEase.SetAnimationStartValue(n.lineW);
	n.lineEase.end = n.lineEase.value;

	int idx = (int)MindMapNodes.size();
	MindMapNodes.push_back(n);

	if (parent >= 0)
	{
		int p = MindMapIndexOf(parent);
		if (p >= 0)
		{
			MindMapNodes[idx].depth = MindMapNodes[p].depth + 1;
			MindMapNodes[p].children.push_back(n.id);
		}
	}
	else
	{
		MindMapNodes[idx].depth = 0;
		MindMapSelect = n.id;
	}

	return n.id;
}

//递归排布节点：子节点统一排在父节点「右侧」，父节点纵向与子节点整体居中。
// 布局分两步：
//   1. 自顶向下：确定每个子节点的横坐标，并让子节点沿纵向依次堆叠；
//   2. 自底向上：把子树占据的纵向包围区回传给父节点，父节点据此把自己的 y
//      对齐到「所有子节点整体的中心」，实现父项永远居中子项。
//parentRight 为父节点底线右端（按钮外侧）的横坐标，parentBottom 为父底线纵坐标；
//outTop/outBottom 回传本子树占据的纵向包围区（含自身及其全部后代）。
static void MindMapLayoutNode(int id, int startY, int& cursorY, int parentRight, int parentBottom,
	int& outTop, int& outBottom)
{
	int idx = MindMapIndexOf(id);
	if (idx < 0) { outTop = outBottom = startY; return; }

	auto& node = MindMapNodes[idx];

	if (node.parent < 0)
	{
		// 根节点固定在起排 Y，横坐标保持创建时的偏左位置
		node.y = startY;
	}
	else
	{
		// 子节点横向：父节点「按钮区右缘」+ 一小段缩进 -> 落在父节点「右侧」。
		// 关键：基准取「按钮区右缘」而非「底线右端」，因为按钮区（+ / 删除 / 调色）
		// 本身就占据了一段横向空间；若以底线右端为基准，按钮区会与子项横向重叠，
		// 从子底线左端出发的连接线也会被按钮压住、视觉上缩成一小截。
		// 以按钮区右缘为基准后，按钮之后的全部元素（子项及其子树）都会整体右移。
		node.x = parentRight + MindMapIndent() / 2;

		// 子节点纵向：从父底线下方开始（父节点「下方」），
		// 多个子节点沿游标依次向下堆叠，保证彼此不重叠。
		//
		// 关键：这里只保证「不压到父底线」——用一个较小的行间距即可。
		// 旧实现用 MindMapRowGap()（父子的起始偏移）作为这里的下限，
		// 结果每个子项都被推离父项一大段；当子项自身还有子项时，
		// 每一层都再叠加一次这个大偏移，空隙迅速累积得非常大。
		//
		// 因此本处（父子行距）与「兄弟堆叠」共用同一个 MindMapSiblingGap()：
		// 它既是「兄弟之间的最小间隔」，也是「父项与子项之间的最小间隔」，
		// 整棵树的纵向节奏由此统一，嵌套多层时不会再逐层放大空隙。
		int minY = parentBottom + MindMapSiblingGap();
		int nodeY = max(cursorY, minY);
		node.y = nodeY;
	}

	// 当前节点「按钮区右缘」，作为其子节点的横向参考点。
	// 注意：用 lineEase.value（缓动后的实际长度）而非 lineW（目标长度），
	// 保证布局基准与屏幕上实际画出的底线/按钮位置一致。
	int myRight = node.x + (int)node.lineEase.value + MindMapBtnZoneW();
	// 当前节点底线纵坐标，作为其子节点的纵向参考点
	int myBottom = node.y + node.lineH + MindMapLineGap();

	// 本子树纵向包围区：初始只含自身（内容顶 -> 底线底）
	int subTop = node.y;
	int subBottom = node.y + node.lineH + MindMapLineGap();

	int nextCursor = node.y + node.lineH;
	for (int c : node.children)
	{
		int childTop = 0, childBottom = 0;
		MindMapLayoutNode(c, startY, nextCursor, myRight, myBottom, childTop, childBottom);
		subTop = min(subTop, childTop);
		subBottom = max(subBottom, childBottom);
	}

	// 自底向上居中：有子节点时，把父节点纵向对齐到「子节点整体中心」。
	// 取子节点包围区中心作为锚点，减去父节点自身内容半高即得父节点 y，
	// 使父节点的内容区在纵向上恰好居中于整组子项。
	if (!node.children.empty())
	{
		int childTop = INT_MAX;
		int childBottom = INT_MIN;
		for (int c : node.children)
		{
			int ci = MindMapIndexOf(c);
			if (ci < 0) continue;
			auto& cn = MindMapNodes[ci];
			childTop = min(childTop, cn.y);
			childBottom = max(childBottom, cn.y + cn.lineH + MindMapLineGap());
		}

		if (childTop != INT_MAX)
		{
			int center = (childTop + childBottom) / 2;
			node.y = center - node.lineH / 2;

			// 根节点不允许被抬到起排线以上，避免顶部溢出画布
			if (node.parent < 0 && node.y < startY) node.y = startY;

			subTop = min(subTop, node.y);
			subBottom = max(subBottom, node.y + node.lineH + MindMapLineGap());
		}
	}

	// 游标推进到本子树最后一个节点底部
	cursorY = nextCursor;
	outTop = subTop;
	outBottom = subBottom;
}

//把「属于某节点包围盒」的笔迹整体平移 (dx, dy)。
// 关键：布局重排会改变节点位置，若不带动手写笔迹，文字会与底线/节点脱节。
//
// 【归属判定：中心点包含，而非包围盒相交】
// 旧实现用「笔迹包围盒与节点包围盒相交」判定归属，这在思维导图场景下是错的：
//   同一层级相邻节点的包围盒纵向只隔一个 MindMapSiblingGap()，而单笔手写
//   笔迹的包围盒会明显超出其所属节点区间；只要某一笔同时压到两个节点的区间，
//   它就会被两个节点同时命中。更关键的是：子节点的横向落点是
//   父按钮区右缘 + 缩进，而父节点横向覆盖到「按钮区右缘」，二者本身就有重叠，
//   于是父项右侧的内容会被判成子节点内容而跟着子项搬走 —— 表现为
//   「父项写的东西新建子项后一半跑到子项去」。
// 现在改为：取笔迹首尾的几何中心，只有中心点落在节点区间内才算属于该节点。
// 中心点在平面内唯一，天然互斥，父子/兄弟不会再重复命中。
//
// 【横向右界：不得外扩按钮区】
// 旧实现把 segR 取到「底线尾 + 按钮区宽度」，理由是"按钮之后的笔迹不属于本节点"。
// 但这个右界恰好等于子节点的横向落点基准（子项 x = 父按钮区右缘 + 缩进），
// 于是父项底线延展后，用户写在延展段 / 紧邻按钮一带的笔迹，中心点会落进
// 「本节点按钮区」与「子节点区间」的公共区域，被判给子节点而跟着搬走 ——
// 表现为「延展区里的东西还是会被搬走」。
// 现在把右界收紧到「底线右端」，只保留一点点笔宽容差，既不吞掉按钮区，
// 也不与子项的横向区间产生重叠，延展段的内容稳定归属父项。
//
// 分组约定：本函数被「按节点分组」调用。详见 MindMapBuildNodeStrokeGroups。
static void MindMapCollectNodeHits(int nodeX, int nodeY, int effW, int lineH,
	vector<pair<int, int>>& outHits)
{
	auto& page = GetCurPage();

	int gap = MindMapLineGap();

	// 当前节点判定区间（世界坐标）
	int segL = nodeX;
	int segR = nodeX + effW + MindMapLineTail();
	int segT = nodeY;
	int segB = nodeY + lineH + gap;

	// 查自身下标，找父节点，算父按钮区右缘
	int selfIdx = -1;
	for (int i = 0; i < (int)MindMapNodes.size(); i++)
	{
		if (MindMapNodes[i].x == nodeX && MindMapNodes[i].y == nodeY)
		{
			selfIdx = i;
			break;
		}
	}

	// 父节点按钮区右缘（横向硬分界线）
	int parentRightEdge = INT_MAX;
	bool hasParent = false;
	if (selfIdx >= 0)
	{
		int pid = MindMapNodes[selfIdx].parent;
		if (pid >= 0)
		{
			int pidx = MindMapIndexOf(pid);
			if (pidx >= 0)
			{
				hasParent = true;
				auto& pn = MindMapNodes[pidx];
				// 与布局基准完全一致：按钮区右缘 = x + lineEase + BtnZoneW
				parentRightEdge = pn.x + (int)pn.lineEase.value + MindMapBtnZoneW();
			}
		}
	}

	for (int i = 0; i < (int)page.Data.size(); i++)
	{
		for (int k = 0; k < (int)page.Data[i].size(); k++)
		{
			auto& d = page.Data[i][k];

			float cx = (d.x + d.x2) * 0.5f;
			float cy = (d.y + d.y2) * 0.5f;

			if (!(d.StartX < 0 && d.StartY < 0))
			{
				cx = (cx + d.StartX) * 0.5f;
				cy = (cy + d.StartY) * 0.5f;
			}

			int px = (int)cx;
			int py = (int)cy;

			bool belongs = false;

			if (hasParent && parentRightEdge != INT_MAX)
			{
				if (px < parentRightEdge)
				{
					// 横向在父按钮区左侧 → 只归父节点
					// 当前如果是子节点，直接跳过
					if (selfIdx >= 0 && MindMapNodes[selfIdx].parent >= 0)
						continue;  // 子节点不抢父左侧的笔迹
					else
						belongs = true;  // 父节点直接收
				}
				else
				{
					// 横向在父按钮区右侧 → 走矩形包含
					belongs = (px >= segL && px <= segR && py >= segT && py <= segB);
				}
			}
			else
			{
				// 根节点：走矩形包含
				belongs = (px >= segL && px <= segR && py >= segT && py <= segB);
			}

			if (belongs)
				outHits.push_back({ i, k });
		}
	}
}

//把一组已命中的笔画（下标为「当前页 Data」索引）整体平移 (offset)。
// 与命中检测分离：增量重排需要先对整棵树做完命中判定、再统一施加位移，
// 否则先移动的笔迹会污染后移动节点的判定基准，产生重复位移。
static void MindMapShiftStrokes(const vector<pair<int, int>>& hits, Vector2i offset)
{
	if (offset.x == 0 && offset.y == 0) return;

	auto& page = GetCurPage();

	for (auto& h : hits)
	{
		// 防御：索引可能因笔画被擦除而失效
		if (h.first < 0 || h.first >= (int)page.Data.size()) continue;
		if (h.second < 0 || h.second >= (int)page.Data[h.first].size()) continue;

		auto& d = page.Data[h.first][h.second];
		d.x += offset.x; d.y += offset.y;
		d.x2 += offset.x; d.y2 += offset.y;
		if (!(d.StartX < 0 && d.StartY < 0))
		{
			d.StartX += offset.x;
			d.StartY += offset.y;
		}
	}
}

//重新计算整棵树位置。
// 与旧实现的关键差异：布局会移动节点，节点包围盒内的手写笔迹必须同步平移，
// 否则重排后文字留在原地、与节点脱节。
//
// 实现要点：把「记录旧几何 -> 重新布局 -> 按包围盒增量搬移笔迹」拆成两步。
//   - MindMapLayoutAll：纯布局，写回节点的 x/y（含子项整体移动）；
//   - MindMapShiftStrokesForDelta：按「新位置 - 旧位置」搬移各节点包围盒内的笔迹。
// 拆分的意义在于 Update 中的「底线延展」也会改变几何，可复用同一套搬移逻辑，
// 无需重复实现包围盒判定。
static void MindMapLayoutAll()
{
	if (MindMapNodes.empty()) return;

	// 找根节点
	int root = -1;
	for (auto& n : MindMapNodes)
		if (n.parent < 0) { root = n.id; break; }

	if (root < 0) return;

	int startY = WindowSize.y / 6;
	int cursor = startY;
	int subTop = 0, subBottom = 0;
	// 根节点无父节点，横/纵参考点传 0 即可（根节点位置由 startY 决定）
	MindMapLayoutNode(root, startY, cursor, 0, 0, subTop, subBottom);
}

// 抹除「指定节点集合」包围盒内的手写笔迹。
static void MindMapEraseStrokes(const vector<int>& ids)
{
	if (ids.empty()) return;

	auto& page = GetCurPage();
	int gap = MindMapLineGap();

	vector<pair<int, int>> hits;

	for (int id : ids)
	{
		int idx = MindMapIndexOf(id);
		if (idx < 0) continue;

		auto& n = MindMapNodes[idx];

		// 父按钮区右缘（当前几何）
		int parentRight = INT_MAX;
		if (n.parent >= 0)
		{
			int pidx = MindMapIndexOf(n.parent);
			if (pidx >= 0)
			{
				auto& pn = MindMapNodes[pidx];
				parentRight = pn.x + (int)pn.lineEase.value + MindMapBtnZoneW();
			}
		}

		int segL = n.x;
		int segR = n.x + (int)n.lineEase.value + MindMapLineTail();
		int segT = n.y;
		int segB = n.y + n.lineH + gap;

		for (int a = 0; a < (int)page.Data.size(); a++)
		{
			for (int b = 0; b < (int)page.Data[a].size(); b++)
			{
				auto& d = page.Data[a][b];

				float cx = (d.x + d.x2) * 0.5f;
				float cy = (d.y + d.y2) * 0.5f;
				if (!(d.StartX < 0 && d.StartY < 0))
				{
					cx = (cx + d.StartX) * 0.5f;
					cy = (cy + d.StartY) * 0.5f;
				}

				int px = (int)cx;
				int py = (int)cy;

				// 父按钮区左侧：子节点不删（保护父项内容）
				if (parentRight != INT_MAX && px < parentRight)
					continue;

				bool inSeg = (px >= segL && px <= segR &&
					py >= segT && py <= segB);

				if (inSeg)
					hits.push_back({ a, b });
			}
		}
	}

	if (hits.empty()) return;

	sort(hits.begin(), hits.end());
	hits.erase(unique(hits.begin(), hits.end()), hits.end());

	// 倒序删除，保证下标有效
	for (int i = (int)hits.size() - 1; i >= 0; i--)
	{
		int a = hits[i].first;
		int b = hits[i].second;

		if (a < 0 || a >= (int)page.Data.size()) continue;
		if (b < 0 || b >= (int)page.Data[a].size()) continue;

		page.Data[a].erase(page.Data[a].begin() + b);
	}
}

// 按「新位置 - 旧位置」把各节点包围盒内的笔迹整体搬移。
static void MindMapShiftStrokesForDelta(const vector<Vector2i>& oldPos,
	const vector<int>& oldEffW,
	const vector<int>& oldLineH)
{
	auto& page = GetCurPage();

	// 计算某个节点在「旧几何」下的父按钮区右缘
	auto ParentRightEdgeOld = [&](int nodeIdx) -> int
		{
			int pid = MindMapNodes[nodeIdx].parent;
			if (pid < 0) return INT_MAX;

			int pidx = MindMapIndexOf(pid);
			if (pidx < 0) return INT_MAX;

			// 在 oldPos / oldEffW 里找父节点对应下标
			for (int j = 0; j < (int)oldPos.size(); j++)
			{
				if (MindMapNodes[pidx].x == oldPos[j].x &&
					MindMapNodes[pidx].y == oldPos[j].y)
				{
					return oldPos[j].x + oldEffW[j] + MindMapBtnZoneW();
				}
			}
			// 兜底：用当前父节点几何
			auto& pn = MindMapNodes[pidx];
			return pn.x + (int)pn.lineEase.value + MindMapBtnZoneW();
		};

	// ---------- 第一遍：收集命中（用旧几何） ----------
	vector<pair<int, int>> hits;
	for (int i = 0; i < (int)MindMapNodes.size(); i++)
	{
		auto& n = MindMapNodes[i];
		int dx = n.x - oldPos[i].x;
		int dy = n.y - oldPos[i].y;
		if (dx == 0 && dy == 0) continue;

		int parentRight = ParentRightEdgeOld(i);

		int segL = oldPos[i].x;
		int segR = oldPos[i].x + oldEffW[i] + MindMapLineTail();
		int segT = oldPos[i].y;
		int segB = oldPos[i].y + oldLineH[i] + MindMapLineGap();

		for (int a = 0; a < (int)page.Data.size(); a++)
		{
			for (int b = 0; b < (int)page.Data[a].size(); b++)
			{
				auto& d = page.Data[a][b];

				float cx = (d.x + d.x2) * 0.5f;
				float cy = (d.y + d.y2) * 0.5f;
				if (!(d.StartX < 0 && d.StartY < 0))
				{
					cx = (cx + d.StartX) * 0.5f;
					cy = (cy + d.StartY) * 0.5f;
				}

				int px = (int)cx;
				int py = (int)cy;

				bool covered = false;

				if (parentRight != INT_MAX)
				{
					if (px < parentRight)
					{
						// 父按钮区左侧：只归父节点
						if (n.parent >= 0)
							continue;   // 当前是子节点，不抢父左侧笔迹
						covered = true; // 当前是父节点，收下
					}
					else
					{
						covered = (px >= segL && px <= segR &&
							py >= segT && py <= segB);
					}
				}
				else
				{
					// 根节点：自身矩形
					covered = (px >= segL && px <= segR &&
						py >= segT && py <= segB);
				}

				if (covered)
					hits.push_back({ a, b });
			}
		}
	}

	// 去重
	sort(hits.begin(), hits.end());
	hits.erase(unique(hits.begin(), hits.end()), hits.end());

	if (hits.empty()) return;

	// ---------- 第二遍：每个笔画取“最深命中节点”的位移 ----------
	for (auto& h : hits)
	{
		int bestDepth = -1;
		Vector2i bestOffset{ 0, 0 };

		for (int i = 0; i < (int)MindMapNodes.size(); i++)
		{
			auto& n = MindMapNodes[i];
			int dx = n.x - oldPos[i].x;
			int dy = n.y - oldPos[i].y;
			if (dx == 0 && dy == 0) continue;

			int parentRight = ParentRightEdgeOld(i);

			int segL = oldPos[i].x;
			int segR = oldPos[i].x + oldEffW[i] + MindMapLineTail();
			int segT = oldPos[i].y;
			int segB = oldPos[i].y + oldLineH[i] + MindMapLineGap();

			if (h.first < 0 || h.first >= (int)page.Data.size()) continue;
			if (h.second < 0 || h.second >= (int)page.Data[h.first].size()) continue;

			auto& d = page.Data[h.first][h.second];

			float cx = (d.x + d.x2) * 0.5f;
			float cy = (d.y + d.y2) * 0.5f;
			if (!(d.StartX < 0 && d.StartY < 0))
			{
				cx = (cx + d.StartX) * 0.5f;
				cy = (cy + d.StartY) * 0.5f;
			}

			int px = (int)cx;
			int py = (int)cy;

			bool covered = false;
			if (parentRight != INT_MAX)
			{
				if (px < parentRight)
				{
					if (n.parent >= 0) continue;
					covered = true;
				}
				else
				{
					covered = (px >= segL && px <= segR &&
						py >= segT && py <= segB);
				}
			}
			else
			{
				covered = (px >= segL && px <= segR &&
					py >= segT && py <= segB);
			}

			if (covered && n.depth > bestDepth)
			{
				bestDepth = n.depth;
				bestOffset = { dx, dy };
			}
		}

		if (bestDepth >= 0)
		{
			vector<pair<int, int>> one{ h };
			MindMapShiftStrokes(one, bestOffset);
		}
	}
}

//重新计算整棵树位置，并把各节点包围盒内的笔迹同步平移。
static void MindMapRelayout()
{
	if (MindMapNodes.empty()) return;

	// 记录重排前的节点位置、实际底线长度与内容高度（三者同源），
	// 供笔迹搬移计算位移量、命中区间与纵向跨度
	vector<Vector2i> oldPos(MindMapNodes.size());
	vector<int> oldEffW(MindMapNodes.size());
	vector<int> oldLineH(MindMapNodes.size());
	for (int i = 0; i < (int)MindMapNodes.size(); i++)
	{
		oldPos[i] = { MindMapNodes[i].x, MindMapNodes[i].y };
		oldEffW[i] = (int)MindMapNodes[i].lineEase.value;
		oldLineH[i] = MindMapNodes[i].lineH;
	}

	MindMapLayoutAll();

	// 按「新位置 - 旧位置」搬移各节点包围盒内的笔迹
	MindMapShiftStrokesForDelta(oldPos, oldEffW, oldLineH);

	// 笔迹被移动后需要整体重绘历史层，否则画布上仍是旧位置的像素
	FlushWriteLayer = true;
}

void MindMap::Reset()
{
	MindMapNodes.clear();
	MindMapSelect = -1;
	MindMapNextId = 0;

	// 建立根节点：横向偏左，纵向中上部
	int x = WindowSize.x / 8;
	int y = WindowSize.y / 6;
	MindMapCreateNode(-1, x, y, User::MainColor);

	// 激活思维导图，此后才允许绘制
	MindMapActived = true;
}

void MindMap::Clear()
{
	MindMapNodes.clear();
	MindMapSelect = -1;
	MindMapNextId = 0;

	// 回到未激活状态：Draw/Update 将不再绘制任何内容，
	// 与画布"擦除全部"的清屏语义保持一致。
	MindMapActived = false;
}

void MindMap::Add()
{
	// 首次添加（用户点击了工具栏"思维导图"）时创建根节点并激活；
	// 激活后 Draw/Update 才会真正生效，未激活时页面不会自动出现思维导图。
	if (!MindMapActived || MindMapNodes.empty())
	{
		MindMap::Reset();
		return;
	}

	// 单实例约束：整张画布只允许存在一个思维导图。
	// 已激活且已存在节点时，再次点击工具栏「思维导图」不应新建第二个根节点，
	// 否则会出现两张互相独立的思维导图（各自有根、各自布局）叠在同一页上。
	// 这里直接弹窗提示并返回，交由用户决定是继续在现有导图上操作还是先删除它。
	//
	// 注意区分两种调用来源：
	//   - 工具栏「思维导图」插件：走本方法，命中此提示；
	//   - 节点上的 + 按钮：走 AddChild()，用于给已有节点追加子项，
	//     语义上是"扩充现有导图"而非"再建一张导图"，因此不受本约束限制。
	Message::ShowMessage("当前页面已存在思维导图，无法重复添加。\n请先删除现有思维导图后再添加。",
		"提示", ICOTYPE_WARNING, { "确定" }, 3, L"MiuBarrd");
	return;
}

int MindMap::AddChild()
{
	// 尚无任何节点（未激活 / 已清屏）时无从挂载子项，直接返回
	if (!MindMapActived || MindMapNodes.empty()) return -1;

	// 挂载到当前选中节点；无有效选中则挂到根节点
	int parentId = MindMapSelect;
	if (MindMapIndexOf(parentId) < 0)
	{
		for (auto& n : MindMapNodes)
			if (n.parent < 0) { parentId = n.id; break; }
	}

	int pIdx = MindMapIndexOf(parentId);
	if (pIdx < 0) return -1;

	auto& parent = MindMapNodes[pIdx];

	// 子节点横坐标按层级缩进
	int x = parent.x + MindMapIndent();
	int y = parent.y + parent.lineH + MindMapRowGap();

	int newId = MindMapCreateNode(parentId, x, y, parent.color);

	// 关键修复：新增子节点后，选中态保持在其父节点上（而非新子节点）。
	// 原因：
	//   1) 用户点「父项 + 」的心理预期是"继续给这个父项加并列子项"，
	//      若把选中态挪到新子项上，连续点击会变成层层嵌套（子 -> 孙 -> 曾孙），
	//      不符合"给同一父项加多个子项"的操作直觉；
	//   2) 旧实现把选中态落到新子项，导致 Update 中"锁定被点父节点长度"的
	//      补偿代码锁错了对象，父项底线仍随 children.size() 增长而自动延展。
	//      虽然底线长度现已与子节点数量解耦，这里仍保持选中态落在父项上，
	//      使行为与长度规则一致、语义清晰。
	MindMapSelect = parentId;

	MindMapRelayout();

	return newId;
}

void MindMap::Del()
{
	if (MindMapNodes.empty()) return;

	int idx = MindMapIndexOf(MindMapSelect);
	if (idx < 0) return;

	// 根节点的删除规则：
	//   只有当整张思维导图「只剩这一个根节点」时才允许删除（并连带清空整张图），
	//   否则保留原有的保护逻辑，避免误删根节点导致整棵子树一起消失。
	if (MindMapNodes[idx].parent < 0)
	{
		if (MindMapNodes.size() != 1) return;

		// 删除唯一根节点等价于清空思维导图：
		// 复位为未激活状态，Draw/Update 将不再绘制任何内容。
		MindMap::Clear();
		return;
	}

	int parentId = MindMapNodes[idx].parent;

	vector<int> delIds;
	MindMapCollectSubtree(MindMapSelect, delIds);

	// 从父节点的 children 列表摘除
	int p = MindMapIndexOf(parentId);
	if (p >= 0)
	{
		auto& ch = MindMapNodes[p].children;
		ch.erase(remove(ch.begin(), ch.end(), MindMapSelect), ch.end());
	}

	// 按编号移除整棵子树（避免边删边导致下标错位）
	for (int id : delIds)
	{
		int i = MindMapIndexOf(id);
		if (i >= 0) MindMapNodes.erase(MindMapNodes.begin() + i);
	}

	MindMapSelect = parentId;
	MindMapRelayout();
}

void MindMap::Update()
{
	// 触控坐标是「屏幕坐标」，而节点存储的是「世界坐标」。
	// 这里把屏幕坐标反变换回世界坐标再做命中检测，
	// 否则平移/缩放画布后，按钮判定仍停留在原位，导致点不到按钮。
	Vector2i screenPos = XMsg::TouchMsg::GetTouchPos();
	bool touching = IsTouching();

	// 尚未激活时（用户从未点过"思维导图"插件），只更新动画，不做交互判定也不绘制。
	// 这样一进页面不会莫名出现一个空白节点。
	//
	// 关键：未激活时也必须推进底线的缓动动画。
	// 原因：Draw 与 Update 都按激活态门控，若此处直接 return，
	// 则在「首次激活前」或「Clear 回到未激活」之后，节点底线的 lineEase
	// 会停在半途不再收敛；等到再次激活绘制时，底线会从残值处突然继续，
	// 表现为动画卡顿/闪跳。这里先跑一次纯动画推进，保证无论是否激活，
	// 缓动状态始终连续、可收敛。
	if (!MindMapActived)
	{
		MindMapBtnHit = false;
		MindMapWasBtnHit = false;

		// 未激活态下仅推进底线缓动（无交互、无命中判定）
		MindMapUpdateLineEase();
		return;
	}

	// 鼠标/触控坐标可能位于窗口外，先做一次合法性检查
	if (screenPos.x < 0 || screenPos.y < 0)
	{
		MindMapBtnHit = false;
	}

	// 只在「按下的一瞬间」触发，避免长按连续添加。
	// 静态状态需保留在函数内部，因此这里用文件级静态变量记录上一次触控状态。
	bool pressed = touching && !MindMapLastTouch;
	MindMapLastTouch = touching;

	// 屏幕坐标 -> 世界坐标（与 Draw 中的 SX/SY 互为逆变换）
	// 约定：屏幕 = 世界 * scale + CameraPos * scale，因此逆变换为
	//       世界 = 屏幕 / scale - CameraPos。
	// 这里用 double 参与中间运算再取整，避免每次缩放后逐级取整导致累积偏移。
	auto& pd = GetCurPage();
	float scale = pd.Scale / 100.0f;
	if (scale <= 0.0001f) return;
	int posX = (int)std::floor(screenPos.x / (double)scale - pd.CameraPos.x);
	int posY = (int)std::floor(screenPos.y / (double)scale - pd.CameraPos.y);

	// 命中检测的坐标策略：与 Draw 完全一致地走「世界坐标 -> 屏幕坐标」正变换。
	//
	// 为什么不用「把触控点反变换到世界坐标」：
	//   Draw 里按钮圆心是 SX(a) + sep + r 逐项取整后相加得到的（见 Draw 中 bx），
	//   SX(a) = (int)(a * scale + camX) 有一次向下取整；
	//   而反变换走的是 screen/scale - CameraPos，是另一条取整路径。
	//   两条路径的取整误差在 scale != 1 时方向与大小都不一致，
	//   缩放到非整数倍后判定圆心与视觉圆心系统性错开（表现为「检测位置左偏」）。
	// 现在改为对每个按钮圆心复用与绘制完全相同的表达式与取整顺序，
	// 保证判定点与视觉点逐像素重合。
	(void)posX; (void)posY;

	// 世界坐标 -> 屏幕坐标（与 Draw 中的 SX/SY 严格一致）
	float camX = pd.CameraPos.x * scale;
	float camY = pd.CameraPos.y * scale;
	auto SX = [&](int x) { return (int)(x * scale + camX); };
	auto SY = [&](int y) { return (int)(y * scale + camY); };

	int r = MindMapBtnR();
	int sep = MindMapBtnSep();
	int gap = MindMapLineGap();
	int pad = MindMapLinePad();

	// ② 先做 + 按钮命中检测（每帧都算，不限于按下瞬间）：
	// 目的是把「光标是否落在按钮上」这一信息暴露给书写系统，
	// 使点击按钮时触控不会"穿透"到底层画布留下笔迹。
	//
	// 关键：按钮的视觉位置由 Draw 按「世界坐标 * scale + CameraPos * scale」映射，
	// 因此这里必须按同一变换把按钮圆心换算到屏幕坐标再判定；
	// 若直接用世界坐标比较，缩放后判定位置会与视觉位置错位（点不中按钮）。
	int hoverBtn = -1;
	int hoverDepth = -1;
	int hoverColorBtn = -1;
	int hoverColorDepth = -1;
	int hoverDelBtn = -1;
	int hoverDelDepth = -1;
	{
		// 命中半径在世界下是 r，映射到屏幕后应随 scale 同步放大；
		// 再加 2 像素的固定容差（屏幕单位），兼顾触控手指的接触面积。
		// 这样命中区与按钮的视觉范围在任何缩放级别下都严格对应。
		int hitR = max(2, (int)(r * scale) + 2);
		int hitR2 = hitR * hitR;

		for (auto& n : MindMapNodes)
		{
			// 按钮圆心（屏幕坐标）：复用与 Draw 完全相同的取整顺序，
			// 即先 SX(底线右端) 取整，再加 sep 与 r —— 避免两条取整路径产生偏移。
			int lineY = SY(n.y + n.lineH + gap);
			int x2 = SX(n.x + (int)n.lineEase.value);
			int bx = x2 + sep + r;
			int by = lineY;

			int dx = screenPos.x - bx, dy = screenPos.y - by;
			if (dx * dx + dy * dy <= hitR2 && n.depth > hoverDepth)
			{
				hoverDepth = n.depth;
				hoverBtn = n.id;
			}

			// 调色按钮：位于 + 按钮右侧（相距 r*2 + 色块间距），与 + 按钮等半径
			// 注意：删除(-)按钮与调色按钮已交换位置 —— 调色按钮现在排在
			// 删除按钮之后（更靠右），因此这里与 Draw 中的绘制顺序保持一致。
			int dbx = bx + r * 2 + MindMapDelBtnSep();
			int ddx = screenPos.x - dbx, ddy = screenPos.y - by;
			if (ddx * ddx + ddy * ddy <= hitR2 && n.depth > hoverDelDepth)
			{
				hoverDelDepth = n.depth;
				hoverDelBtn = n.id;
			}

			// 调色按钮：位于删除按钮右侧（相距 r*2 + 色块间距），与 + 按钮等半径
			int cbx = dbx + r * 2 + MindMapColorBtnSep();
			int cdx = screenPos.x - cbx, cdy = screenPos.y - by;
			if (cdx * cdx + cdy * cdy <= hitR2 && n.depth > hoverColorDepth)
			{
				hoverColorDepth = n.depth;
				hoverColorBtn = n.id;
			}
		}
	}

	bool CanAddLineLengh = true;

	// 全局标记：本帧触控是否落在思维导图的 + 按钮 / 调色按钮上。
	// 书写系统（WriteSub）会据此跳过落笔，避免点击按钮时画出多余笔画。
	//
	// 关键：这里用「本次触控周期内是否曾命中过按钮」做**粘滞**标记。
	// 原因：WriteSub 与 MindMap::Update 在同一帧内的执行顺序可能使本帧尚未
	// 计算出命中结果，而手指按下后的一两帧内坐标会轻微漂移、可能短暂移出按钮，
	// 若仅按当帧瞬时命中判定，就会出现「按住按钮拖动时中途漏屏蔽」的书写穿透。
	// 一旦在本次触控周期内命中过按钮，就屏蔽到手指抬起为止，可彻底杜绝穿透。
	bool curHit = (hoverBtn >= 0 || hoverColorBtn >= 0 || hoverDelBtn >= 0);
	if (touching)
	{
		if (curHit) MindMapWasBtnHit = true;
	}
	else
	{
		// 手指抬起：结束本次触控周期，重置粘滞标记
		MindMapWasBtnHit = false;
	}
	MindMapBtnHit = MindMapWasBtnHit;

	// 点击 + 按钮或调色按钮后，本节点底线不应再「自动延展」。
	//
	// 为什么不能只在按下那一帧置 false：
	//   点按钮后弹出色盘或立刻新增子节点，用户手指/鼠标常在按钮上停留或轻微移动，
	//   同一次触控周期内 WriteSub 仍会产生少量笔迹点（WriteDataTemp 非空），
	//   而此前 CanAddLineLengh 只对「按下帧」关闭，后续帧又恢复为 true，
	//   于是这些残留笔迹的包围盒命中了本节点，把 target 撑大 —— 表现为
	//   「一点 + 或选色，底线就自己变长」。
	// 修复：把抑制条件扩展到「本次触控周期内曾命中过按钮」的整个区间。
	bool onBtn = (hoverBtn >= 0 || hoverColorBtn >= 0 || hoverDelBtn >= 0);

	if (pressed)
	{
		// 仅在真正命中按钮时才抑制延展；点在空白处不应影响底线的正常书写延展
		if (onBtn) MindMapBtnHold = true;
	}
	else if (touching && onBtn)
	{
		// 触控仍在按钮上：持续抑制，避免弹窗/新增过程中的残留笔迹撑长底线
		MindMapBtnHold = true;
	}
	// 抬起：结束本次按钮按住周期（下一帧 WriteDataTemp 已被 WriteSub 清空，安全）
	if (!touching) MindMapBtnHold = false;

	// 按钮按住期间（含抬起当帧）一律不延展底线：
	// 让抑制覆盖到「抬手提交残留笔迹」的那一帧，彻底避免点 + 后底线自己变长。
	if (MindMapBtnHold) CanAddLineLengh = false;

	if (pressed)
	{
		if (hoverColorBtn >= 0)
		{
			// 点击调色按钮：只登记「调色请求」，不在此处弹窗。
			// 原因：Update 没有窗口参数，而 ChooseColorWindow 需要 RenderWindow&；
			// 实际弹窗在 Write::Show(window) 中通过 TakeColorRequest() 取出后执行。
			MindMapSelect = hoverColorBtn;
			MindMapColorRequest = hoverColorBtn;
		}
		else if (hoverBtn >= 0)
		{
			// 点击 + 按钮：选中该节点并给它追加一个子节点。
			// 走 AddChild() 而非 Add()：Add() 是工具栏入口，带单实例约束
			// （已存在导图时会弹提示并拒绝），而 + 按钮的语义是扩充现有导图，
			// 必须直接追加子项。
			MindMapSelect = hoverBtn;
			MindMap::AddChild();

			// 注意：这里不再需要"锁定父底线长度"的补偿逻辑。
			// 底线长度已改为只由「初始长度 + 用户真实书写包围盒」决定，
			// 与子节点数量解耦，因此新增子项不会再把父底线撑长。

			// AddChild 内部已保持选中态落在被点击的父节点上（便于连续并列添加），
			// 这里再显式赋一次，确保后续连续点击 + 始终给同一父项加并列子项。
			MindMapSelect = hoverBtn;
		}
		else if (hoverDelBtn >= 0)
		{
			// 点击 - 按钮：删除该节点及其整棵子树（根节点会被 Del 内部保护）。
			// 关键：点击「父项」的 - 按钮时，其旗下全部子项（含孙项）必须一并删除。
			// 做法是先按子树收集要删除的节点编号，再把子树内笔迹抹掉，最后交给 Del()
			// 递归摘除节点 —— 否则节点没了、手写内容仍留在画布上，形成「删除不干净」。
			MindMapSelect = hoverDelBtn;
			{
				vector<int> delIds;
				MindMapCollectSubtree(hoverDelBtn, delIds);
				MindMapEraseStrokes(delIds);
			}
			MindMap::Del();

			// 删除后重建历史书写层，避免已抹除的笔迹仍残留在画布像素上
			FlushWriteLayer = true;
		}
		else
		{
			// 普通点选底线区域：切换选中节点。
			// 同样改到屏幕坐标下判定（与按钮命中口径一致），
			// 避免缩放后「点底线选不中、点空白却选中」的错位。
			int best = -1;
			int bd = -1;
			for (auto& n : MindMapNodes)
			{
				int lx = SX(n.x - pad);
				int ly = SY(n.y - pad);
				int lw = SX(n.x + n.lineW + pad) - lx;
				int lh = SY(n.y + n.lineH + gap + pad) - ly;
				if (screenPos.x >= lx && screenPos.x <= lx + lw &&
					screenPos.y >= ly && screenPos.y <= ly + lh && n.depth > bd)
				{
					bd = n.depth;
					best = n.id;
				}
			}
			if (best >= 0) MindMapSelect = best;
		}
	}

	// 底线动态延展。基础长度恒为初始长度，实时书写时再按「属于该节点的笔迹」
	// 单独延展，避免多个节点互相干扰。
	//
	// 关键修复：底线不再随子节点数量变长。
	// 旧实现为 baseW + children.size() * perChild，导致父节点每新增一个子项，
	// 底线目标长度就增加一段 perChild；又因「长度只增不减」，表现为
	// 「子项变多后，点父项 + 新建子项，父项底线会自动延展」。
	// 现在把长度与 children.size() 解耦，只由初始长度和用户真实笔迹决定。
	// （lineW 为目标长度，lineEase.value 为当前长度，二者由下方缓动衔接。）
	int baseW = MindMapLineInitW();

	// 收集当前「本次书写」的笔迹包围盒（世界坐标）。
	// 注意：不能把所有已落笔笔画都统计进来，否则任一节点的内容都会让所有节点一起延展。
	int writeRight = INT_MIN;
	int writeLeft = INT_MAX;
	int writeTop = INT_MAX;
	int writeBottom = INT_MIN;
	for (auto& d : WriteDataTemp)
	{
		writeLeft = min(writeLeft, (int)min(d.x, d.x2));
		writeRight = max(writeRight, (int)max(d.x, d.x2));
		writeTop = min(writeTop, (int)min(d.y, d.y2));
		writeBottom = max(writeBottom, (int)max(d.y, d.y2));
	}
	const bool hasWrite = !WriteDataTemp.empty();

	// 记录位置与「实际底线长度」的旧快照：用于判断本轮底线是否被延展，
	// 以及延展后按包围盒把子项与笔迹一并搬移。
	vector<Vector2i> oldPos(MindMapNodes.size());
	vector<int> oldEffW(MindMapNodes.size());
	for (int i = 0; i < (int)MindMapNodes.size(); i++)
	{
		oldPos[i] = { MindMapNodes[i].x, MindMapNodes[i].y };
		oldEffW[i] = (int)MindMapNodes[i].lineEase.value;
	}

	if (CanAddLineLengh)
	{
		for (auto& n : MindMapNodes)
		{
			int target = baseW;

			// 只有「当前选中节点」才允许被本笔画延展
			if (n.id == MindMapSelect)
			{
				target += MindMapLinePad();

				if (hasWrite)
				{
					// 当前节点的横向区间（不扩按钮区）
					int segL = n.x;
					int segR = n.x + n.lineW + MindMapLineTail();

					// 纵向区间：内容区到底线
					int contentUp = n.lineH / 2;
					int segT = n.y - contentUp;
					int segB = n.y + n.lineH + gap;

					// 笔迹包围盒与本节点区间相交
					bool inSeg = !(writeRight < segL || writeLeft > segR ||
						writeBottom < segT || writeTop > segB);

					if (inSeg)
					{
						int need = writeRight - n.x + MindMapLineTail();
						if (need > target) target = need;
					}
				}
			}

			// 长度只增不减
			if (target < n.lineW) target = n.lineW;
			n.lineW = target;
		}
	}

	// 底线动态拓展：以缓动方式逼近各节点的目标长度（参照对齐线的动画逻辑）。
	// 关键：lineEase.value 才是「当前实际长度」，lineW 是目标长度。
	// 这里复用 MindMapUpdateLineEase()，与未激活分支走同一套推进逻辑，
	// 保证「激活 / 未激活」两种状态下的动画行为完全一致、状态连续。
	MindMapUpdateLineEase();

	// 底线延展后，父项「按钮区右缘」右移，子项及其子树必须整体右移；
	// 否则子项会被父项变长后的底线/按钮压住，连接线跨度也会被压缩。
	//
	// 关键：这里按「布局基准」而不是「缓动值」判断是否需要重排。
	// 布局里子节点 x 取自父节点 lineEase.value（当前长度），缓动每帧都在变，
	// 若按缓动值逐帧重排，会变成每帧都在微调子项位置（抖动且开销大）。
	// 因此只在「目标长度」发生实质变化时才重排一次，重排时布局自动读取
	// 当时的 lineEase.value，子项会平滑地跟着父项底线一起右移。
	bool layoutDirty = false;
	for (int i = 0; i < (int)MindMapNodes.size(); i++)
	{
		if (MindMapNodes[i].lineW != oldEffW[i] && MindMapNodes[i].lineW != MindMapLineInitW())
		{
			layoutDirty = true;
			break;
		}
	}

	// 底线变化会改变子项落点：重排（同时按包围盒搬移笔迹）。
	// 注意：重排只做一次，避免与缓动同频抖动；笔迹搬移在重排内部完成，
	// 因此「父项写字 -> 底线自动延展 -> 子项与笔迹一起右移」形成完整闭环。
	if (layoutDirty) MindMapRelayout();
}

bool MindMap::IsBtnHit()
{
	return MindMapBtnHit;
}

int MindMap::TakeColorRequest()
{
	// 取出后立即复位：同一次点击只应触发一次调色窗口
	int id = MindMapColorRequest;
	MindMapColorRequest = -1;
	return id;
}

void MindMap::Draw(RenderTarget& dest)
{
	// 未激活（用户还没点过"思维导图"）时不绘制，避免自动出现
	if (!MindMapActived) return;

	if (MindMapNodes.empty()) return;

	int r = MindMapBtnR();
	int sep = MindMapBtnSep();
	int gap = MindMapLineGap();
	int lw = max(2, WindowSize.y / 260);

	// 坐标变换：与笔画一致，世界坐标经 CameraPos 平移、Scale 缩放后落到屏幕，
	// 这样平移/缩放画布时思维导图会跟随移动，不会"钉死"在屏幕上。
	auto& pd = GetCurPage();
	float scale = pd.Scale / 100.0f;
	float camX = pd.CameraPos.x * scale;
	float camY = pd.CameraPos.y * scale;

	// 世界坐标 -> 屏幕坐标
	auto SX = [&](int x) { return (int)(x * scale + camX); };
	auto SY = [&](int y) { return (int)(y * scale + camY); };

	// 每帧直接绘制到目标，避免离屏图层的脏标记导致内容只显示一帧就消失
	// ① 先画父子连接线（颜色取父节点，完全不透明）
	// 样式（单条平滑曲线，无竖线）：
	// 默认以一条三次贝塞尔从「父节点按钮之后」连到「子底线头（左端）」上方；
	// 当父项只有一个子项、或父子底线纵向重合（同一 y）时，改用一条水平实线直连，
	// 使「一对一」与「同层平铺」这两种简单情形呈现更干净、更符合直觉的直线连接。
	//
	// 控制点的取法决定了曲线的观感：
	//   - 第一个控制点排在父端右侧、纵向基本不抬升，使出笔方向贴近水平，起步平缓；
	//   - 第二个控制点排在子端左侧、纵向基本不抬升，使收笔方向同样贴近水平，落笔平缓；
	//   - 两控制点横向分处两端、纵向差被刻意压小，使整条曲线呈现"平滑 S 形"，
	//     而不是旧实现那种中段过陡、近似对角的折线感。
	//
	// 父端起点：定位到「父项右侧全部按钮之后」，保证连线不从按钮中间穿出；
	// 子端终点：相对子底线左端上移 detachC，与子项留出明显空隙。
	XGraph::LineShape::SetLineWidth(lw);
	for (auto& n : MindMapNodes)
	{
		// 父节点起点：位于父项「三个按钮全部之后」（+ / 删除(-) / 调色）再留一段空隙。
		// 底线尾 = n.x + lineEase.value；按钮区总宽度由 MindMapBtnZoneW() 统一给出，
		// 这样按钮尺寸 / 间距调整时起点会自动跟着右移，不会压在按钮上。
		//
		// 与之配套：子节点的横向落点同样以「按钮区右缘」为基准
		// （见 MindMapLayoutNode 中子节点 x 的计算），因此按钮区变宽时
		// 子项会整体右移，连接线始终有完整跨度，不会被压成"指向箭头"。
		int pTail = n.x + (int)n.lineEase.value;
		int afterBtn = pTail + MindMapBtnZoneW() + MindMapLinkStartPad();
		int px = SX(afterBtn);
		int py = SY(n.y + n.lineH + gap);

		for (int c : n.children)
		{
			int ci = MindMapIndexOf(c);
			if (ci < 0) continue;

			auto& child = MindMapNodes[ci];

			// 子节点底线「头」（左端）作为终点
			int cx = SX(child.x);
			int cy = SY(child.y + child.lineH + gap);

			// 连线使用完全不透明的父节点颜色
			Color lc = n.color;
			lc.a = 255;
			XGraph::SetColor(lc);

			// 子端断开：终点在子底线左端上方止步，缝隙随缩放同步缩放。
			int detachC = max(3, (int)(MindMapLinkDetach() * scale));

			// 实线判定：父项只有一个子项，或父子底线纵向几乎重合（同一 y）时，
			// 直接用一条水平实线连接，简洁直观。
			// 用屏幕坐标下的纵向差做判定，并给一点容差，避免取整误差导致漏判。
			bool straightLine = (n.children.size() == 1) || (abs(cy - py) <= max(2, (int)(4 * scale)));

			if (straightLine)
			{
				// 水平实线：从父端按钮之后直连到子底线左端（略上移 detachC 留缝）
				XGraph::LineShape::Line((float)px, (float)py, (float)cx, (float)(cy - detachC), dest);
				continue;
			}

			float x0 = (float)px;
			float y0 = (float)py;
			float x3 = (float)cx;
			float y3 = (float)(cy - detachC);

			// 采样点数：足够密以保证曲线平滑，又不至于在极端缩放时产生过多 draw call
			const int SEG = 32;

			// 控制点横向推进量：取水平跨度的 0.55 倍，使曲线腰部过渡更舒展、不易自交。
			float hSpan = (float)abs(cx - px);
			float ctrl = hSpan * 0.55f;
			if (ctrl < 1.0f) ctrl = 1.0f;

			// 控制点纵向位置：贴着两端（各取纵向差的 1/5），让两端切线更接近水平，
			// 从而形成"起笔平缓 - 中段平滑上扬 - 收笔平缓"的柔和 S 形，
			// 相比旧实现的 1/3 取值，中段腰部的拐折明显更缓、更美观。
			float c1x = x0 + ctrl;
			float c1y = y0 + (y3 - y0) * 0.2f;
			float c2x = x3 - ctrl;
			float c2y = y3 - (y3 - y0) * 0.2f;

			// 三次贝塞尔单点求值：P0/P3 为端点，P1/P2 为控制点
			auto CubicAt = [](float p0, float p1, float p2, float p3, float t)
				{
					float mt = 1.0f - t;
					return mt * mt * mt * p0
						+ 3.0f * mt * mt * t * p1
						+ 3.0f * mt * t * t * p2
						+ t * t * t * p3;
				};

			// 逐段采样绘制这条单段三次贝塞尔（全程无竖直/水平直段）
			float lastX = x0;
			float lastY = y0;

			for (int s = 1; s <= SEG; ++s)
			{
				float t = (float)s / SEG;
				float nx = CubicAt(x0, c1x, c2x, x3, t);
				float ny = CubicAt(y0, c1y, c2y, y3, t);
				XGraph::LineShape::Line(lastX, lastY, nx, ny, dest);
				lastX = nx; lastY = ny;
			}
		}
	}

	// ② 再画每个节点：底线 + 右端圆形按钮 + 加号
	for (auto& n : MindMapNodes)
	{
		int lineY = SY(n.y + n.lineH + gap);
		int x1 = SX(n.x);
		int x2 = SX(n.x + (int)n.lineEase.value);

		// 永不消逝的底线（固定颜色）
		XGraph::SetColor(n.color);
		XGraph::LineShape::SetLineWidth(lw);
		XGraph::LineShape::Line(x1, lineY, x2, lineY, dest);

		// + 按钮：与底线分离，位于底线右端再向右 sep 处，半径更小
		int bx = x2 + sep + r;
		int by = lineY;
		XGraph::SetFillColor(n.color);
		XGraph::CircleShape::FillCircle_WithoutBorder(bx, by, r, dest);

		// 圆上的白色加号
		int arm = (int)(r * 0.5f);
		int aw = max(2, lw);
		XGraph::SetColor(Color::White);
		XGraph::LineShape::SetLineWidth(aw);
		XGraph::LineShape::Line(bx - arm, by, bx + arm, by, dest);
		XGraph::LineShape::Line(bx, by - arm, bx, by + arm, dest);

		// 删除(-)按钮：位于 + 按钮右侧，圆形背景 + 深色减号，用于删除该节点及其子树
		// 注意：本按钮与调色按钮已交换位置 —— 现在删除按钮排在 + 之后、调色之前。
		int dbx = bx + r * 2 + MindMapDelBtnSep();
		int dby = by;
		XGraph::SetFillColor(Color(240, 240, 240));
		XGraph::CircleShape::FillCircle_WithoutBorder(dbx, dby, r, dest);
		// 圆上的减号：使用 Color(30,30,30) 深色显示，保证在浅色按钮底上清晰可辨
		XGraph::SetColor(Color(30, 30, 30));
		XGraph::LineShape::SetLineWidth(aw);
		XGraph::LineShape::Line(dbx - arm, dby, dbx + arm, dby, dest);

		// 调色按钮：位于删除按钮右侧，圆形背景 + 中心色块，用于自定义该节点颜色
		int cbx = dbx + r * 2 + MindMapColorBtnSep();
		int cby = by;
		XGraph::SetFillColor(Color(240, 240, 240));
		XGraph::CircleShape::FillCircle_WithoutBorder(cbx, cby, r, dest);
		// 中心色块填充当前节点颜色，直观表示该节点的配色
		XGraph::SetFillColor(n.color);
		XGraph::CircleShape::FillCircle_WithoutBorder(cbx, cby, max(2, r - max(2, lw)), dest);
	}
}

#pragma endregion


//柳叶笔
#pragma region MyRegion

void DrawFilledPolygon(
	const vector<Vector2i>& points,
	const Color& color,
	RenderTarget& target)
{
	if (points.size() < 3) return;

	// 把 Vector2i 转成 Vector2f 给 ConvexShape 用
	sf::ConvexShape polygon;
	polygon.setPointCount(points.size());
	for (size_t i = 0; i < points.size(); ++i)
	{
		polygon.setPoint(i, sf::Vector2f(
			static_cast<float>(points[i].x),
			static_cast<float>(points[i].y)
		));
	}

	polygon.setFillColor(color);
	polygon.setOutlineThickness(0); // 无边框

	target.draw(polygon);
}

#pragma endregion

//擦除
#pragma region MyRegion

//草稿层
bool TempLayer = false;

//放大橡皮擦
void ExEraseSize(bool Ex)
{
	static int size = 0;

	if(Ex)
	{
		if(Write::EraseSize != EraseBasicSize * 2)
		{
			size = Write::EraseSize;
			Write::EraseSize = EraseBasicSize * 2;
		}
	}
	else 
	{
		if (Write::EraseSize != size)
		{
			if (size > 0)
				Write::EraseSize = size;
		}
	}

}

bool IsErase()
{
	// 多指 ，始终擦除
	if (TouchNum > 5)
	{
		ExEraseSize(true);
		return true;
	}

	// 单指/鼠标 ， 需要工具是橡皮
	if (Tool::ToolCount == 1 && (TouchNum == 1 || XMsg::KeyMsg::Keystate(VK::MouseLeft)) && TouchNum < 2)
	{
		ExEraseSize(false);

		return true;
	}

	return false;
}

//是否需要推入撤销栈
bool NeedPushUnDoStack = true;

 // 线段与矩形相交（含端点在内）
static bool LineHitRect(int x1, int y1, int x2, int y2, int rx, int ry, int rw, int rh)
{
	// 端点之一在矩形内
	if ((x1 >= rx && x1 <= rx + rw && y1 >= ry && y1 <= ry + rh) ||
		(x2 >= rx && x2 <= rx + rw && y2 >= ry && y2 <= ry + rh))
		return true;

	// 跨立实验：线段与矩形四边逐一检测
	int e[4][4] = {
		{rx, ry, rx + rw, ry},          // 上
		{rx, ry + rh, rx + rw, ry + rh}, // 下
		{rx, ry, rx, ry + rh},           // 左
		{rx + rw, ry, rx + rw, ry + rh}  // 右
	};

	for (int i = 0; i < 4; i++)
	{
		int ax = x2 - x1, ay = y2 - y1;
		int bx = e[i][2] - e[i][0], by = e[i][3] - e[i][1];
		int dx = e[i][0] - x1, dy = e[i][1] - y1;

		int d = ax * by - ay * bx;
		if (d == 0) continue; // 平行

		float t = (float)(dx * by - dy * bx) / d;
		float u = (float)(dx * ay - dy * ax) / d;

		if (t >= 0 && t <= 1 && u >= 0 && u <= 1)
			return true;
	}
	return false;
}

static int EraseStf = WindowSize.x;
bool NeedErase(const int& sx1, const int& sy1, const int& sx2, const int& sy2, Vector2i& ErasePos, bool IsLeaf = false, const int& ssx1 = -1, const int& ssy1 = -1)
{
	if (!IsErasing || Tool::IsInBar) return false;

	int rw = Write::EraseSize * 2 / ScreenScale * EraseStf,rh = Write::EraseSize * 2.8 / ScreenScale * EraseStf;

	//推入撤销栈
	if (NeedPushUnDoStack)
	{
		auto& page = GetCurPage();
		auto& data = page.Data;

		if (!data.empty() && !data.back().empty())
		{
			page.UnDoData.push({ static_cast<int>(data.size() - 1), data.back() });
		}
		else
		{
			NeedPushUnDoStack = false;
			return false; // 或者跳过擦除逻辑
		}

		NeedPushUnDoStack = false;
	}

	if(!IsLeaf)
	{
		if (sx1 > ErasePos.x - rw / 2 && sy1 > ErasePos.y - rh / 2 && sx1 < ErasePos.x + rw && sy1 < ErasePos.y + rh)
			return true;
		if (sx2 > ErasePos.x - rw / 2 && sy2 > ErasePos.y - rh / 2 && sx2 < ErasePos.x + rw && sy2 < ErasePos.y + rh)
			return true;
	}
	else if(ssx1 > -1 && ssy1 > -1)
	{
		if (sx1 > ErasePos.x - rw / 2 && sy1 > ErasePos.y - rh / 2 && sx1 < ErasePos.x + rw && sy1 < ErasePos.y + rh)
			return true;
		if (sx2 > ErasePos.x - rw / 2 && sy2 > ErasePos.y - rh / 2 && sx2 < ErasePos.x + rw && sy2 < ErasePos.y + rh)
			return true;

		if (LineHitRect(ssx1, ssy1, sx1, sy1, ErasePos.x, ErasePos.y, rw, rh)) return true;
		if (LineHitRect(ssx1, ssy1, sx2, sy2, ErasePos.x, ErasePos.y, rw, rh)) return true;
	}

	return false;
}

//绘制橡皮擦
void DrawEraseRect(RenWin& window)
{
	if (!IsErasing || Tool::IsInBar) return;
	
	Vector2i erasePos = XMsg::MouseMsg::GetMousePosWindow();

	int ex = erasePos.x - Write::EraseSize * EraseStf / ScreenScale;
	int ey = erasePos.y - Write::EraseSize * 1.4 * EraseStf / ScreenScale;

	static int r = WindowSize.x / 200;

	XGraph::SetFillColor(Color(250, 250, 250));
	XGraph::RectangleShape::FillRoundRect_WithoutBorder(ex, ey, Write::EraseSize * 2 * EraseStf / ScreenScale, Write::EraseSize * 2.8 * EraseStf / ScreenScale, r,window);
	XGraph::SetColor(Color(220, 220, 220));
	static int lw = WindowSize.x / 130;
	XGraph::LineShape::SetLineWidth(lw);
	XGraph::LineShape::Line(ex + Write::EraseSize * 0.6 * EraseStf / ScreenScale,
		ey + Write::EraseSize * 0.4 * EraseStf / ScreenScale,
		ex + Write::EraseSize * 0.6 * EraseStf / ScreenScale, 
		ey + Write::EraseSize * 2.4 * EraseStf / ScreenScale, window);
	XGraph::LineShape::Line(ex + Write::EraseSize * 1.4 * EraseStf / ScreenScale, 
		ey + Write::EraseSize * 0.4 * EraseStf / ScreenScale,
		ex + Write::EraseSize * 1.4 * EraseStf / ScreenScale, 
		ey + Write::EraseSize * 2.4 * EraseStf / ScreenScale, window);

	WriteStopTime = 10;
}

#pragma endregion

//绘制
#pragma region MyRegion


//绘制书写层
void DrawLayer(RenderTarget& window)
{
	auto& pd = GetCurPage();

	// 仅当"内容变化"（新笔画入栈/撤销/擦除/换页）时才全量重绘历史层；
	// 相机平移/缩放时历史笔画只是整体位移，直接复用已有纹理并整体平移贴图，
	// 避免每帧对全部历史笔画重新跑一遍 CPU 遍历 + GPU 光栅（移动卡顿的主因）。
	static Vector2i CamPosTemp;
	static int ScaleTemp = -1;
	const bool contentDirty = FlushWriteLayer || IsErasing;

	Vector2i ErasePos = XMsg::MouseMsg::GetMousePosWindow();

	if (contentDirty)
	{
		WriteLayer.clear(Color::Transparent);
		LightLayer.clear(Color::Transparent);

		bool IsLight = false;

		// SDF 渲染器可用时优先使用距离场绘制（解析 AA、每像素只算一次距离）；
		// 不可用时回退到原有的 XBatch 批量提交路径。
		bool useSDF = SDFLineRenderer::Instance().Ready();

		// 当前 SDF 批次的目标层（普通层或荧光层）
		bool BatchOnLight = false;
		bool BatchOpened = false;

		if (useSDF)
		{
			// 延迟到遇到第一条线段时才 Begin，避免空批次
		}
		else XBatch::begin(WriteLayer);

		float scale = pd.Scale / 100.0f;  // 缩放因子（100 = 原尺寸）

		float camScaleX = pd.CameraPos.x * scale;
		float camScaleY = pd.CameraPos.y * scale;

		int Size = pd.Data.size();
		for (int i = 0; i < Size; ++i)
		{
			//抽样
			static const int CHECKRECTBLOCK = 25;
			bool NeedDraw = false;

			//缓存 size()，避免循环中反复调用
			int dataSize = (int)pd.Data[i].size();
			int block = min(CHECKRECTBLOCK, dataSize / 4);
			if (block > 0)
			{
				for (int c = 0; c < dataSize; c += block)
				{
					auto& data = pd.Data[i][c];

					float sx1 = data.x * scale + camScaleX;
					float sy1 = data.y * scale + camScaleY;
					float sx2 = data.x2 * scale + camScaleX;
					float sy2 = data.y2 * scale + camScaleY;
					float ssx1 = data.StartX * scale + camScaleX;
					float ssy1 = data.StartY * scale + camScaleY;

					if (sx1 < 0 && sx2 < 0) continue;
					if (sy1 < 0 && sy2 < 0) continue;
					if (sy1 > WindowSize.y && sy2 > WindowSize.y) continue;
					if (sx1 > WindowSize.x && sx2 > WindowSize.x) continue;

					NeedDraw = true;
					break;
				}
			}
			else NeedDraw = true;

			if(NeedDraw)
			{
				for (int k = 0; k < pd.Data[i].size(); ++k)
				{
					auto& data = pd.Data[i][k];

					float sx1 = data.x * scale + camScaleX;
					float sy1 = data.y * scale + camScaleY;
					float sx2 = data.x2 * scale + camScaleX;
					float sy2 = data.y2 * scale + camScaleY;
					float ssx1 = data.StartX * scale + camScaleX;
					float ssy1 = data.StartY * scale + camScaleY;

					if (sx1 < 0 && sx2 < 0) continue;
					if (sy1 < 0 && sy2 < 0) continue;
					if (sy1 > WindowSize.y && sy2 > WindowSize.y) continue;
					if (sx1 > WindowSize.x && sx2 > WindowSize.x) continue;

					//擦除逻辑
					if (!TempLayer || data.TempLayer)
					{
						if (data.StartX < 0 && data.StartY < 0)
						{
							if (NeedErase(sx1, sy1, sx2, sy2, ErasePos))
							{
								swap(pd.Data[i][k], pd.Data[i].back());
								pd.Data[i].pop_back();
								--k;
								continue;
							}
						}
						else
						{
							if (NeedErase(sx1, sy1, sx2, sy2, ErasePos, true, ssx1, ssy1))
							{
								swap(pd.Data[i][k], pd.Data[i].back());
								pd.Data[i].pop_back();
								--k;
								continue;
							}
						}
					}

					bool shouldBeLight = data.Light;

					if (shouldBeLight != IsLight)
					{
						if (useSDF)
						{
							// SDF 批次必须切换目标层前冲刷，因为一次 draw 只能作用于一个目标
							if (BatchOpened && BatchOnLight != shouldBeLight)
							{
								SDFBatchFlush(BatchOnLight ? LightLayer : WriteLayer);
								BatchOpened = false;
							}
						}
						else
						{
							XBatch::end();
							if (shouldBeLight)
								XBatch::begin(LightLayer);
							else
								XBatch::begin(WriteLayer);
						}
						IsLight = shouldBeLight;
					}

					if (data.StartX < 0 && data.StartY < 0)
					{
						if (useSDF)
						{
							sf::RenderTarget& layer = shouldBeLight ? (sf::RenderTarget&)LightLayer : (sf::RenderTarget&)WriteLayer;
							float halfW = max(data.w * scale * 0.5f, 0.5f);

							// 颜色或线宽变化时要重开批次，保证同一批次参数一致
							if (!BatchOpened)
							{
								SDFBatchBegin(data.color, halfW, shouldBeLight ? (150.f / 255.f) : 1.f);
								BatchOpened = true;
								BatchOnLight = shouldBeLight;
							}
							else if (std::abs(halfW - g_SDFHalfW) > 0.01f ||
								g_SDFColor.r != data.color.r || g_SDFColor.g != data.color.g ||
								g_SDFColor.b != data.color.b)
							{
								SDFBatchFlush(layer);
								SDFBatchBegin(data.color, halfW, shouldBeLight ? (150.f / 255.f) : 1.f);
							}

							SDFBatchPush(layer, sx1, sy1, sx2, sy2);
						}
						else
						{
							XGraph::LineShape::SetLineWidth(data.w * scale);
							XGraph::SetColor(data.color);
							XGraph::LineShape::Line(
								sx1, sy1, sx2, sy2,
								shouldBeLight ? LightLayer : WriteLayer);
						}
					}
					else
					{
						// 柳叶笔是三角形填充，SDF 线段着色器覆盖不到，单独走三角光栅
						if (useSDF)
						{
							if (BatchOpened)
							{
								SDFBatchFlush(BatchOnLight ? LightLayer : WriteLayer);
								BatchOpened = false;
							}
						}
						else XBatch::end();

						DrawFilledPolygon({ {(int)ssx1,(int)ssy1},{(int)sx1,(int)sy1},{(int)sx2,(int)sy2} }, data.color, WriteLayer);

						if (!useSDF) XBatch::begin(WriteLayer);
					}

				}
			}
		}


		if (useSDF)
		{
			// 收尾：冲刷可能残留的 SDF 批次
			if (BatchOpened) SDFBatchFlush(BatchOnLight ? LightLayer : WriteLayer);
		}
		else XBatch::end();

		WriteLayer.display();LightLayer.display();

		CamPosTemp = pd.CameraPos;
		ScaleTemp = pd.Scale;
		FlushWriteLayer = false;
	}

	//注意：不能用 static——WriteLayer 在窗口尺寸/设置变化时会被整体重建，
	//静态 Sprite 会一直持有已析构的旧纹理，导致绘制失效。
	Sprite s(WriteLayer.getTexture());

	//相机平移时历史纹理整体偏移，避免全量重算
	if (!contentDirty && ScaleTemp == pd.Scale && CamPosTemp != pd.CameraPos)
	{
		float scale = pd.Scale / 100.0f;
		const float dx = (pd.CameraPos.x - CamPosTemp.x) * scale;
		const float dy = (pd.CameraPos.y - CamPosTemp.y) * scale;
		s.move(sf::Vector2f(dx, dy));
		CamPosTemp = pd.CameraPos;
	}

	window.draw(s);
}
//绘制荧光层
void DrawLightLayer(RenderTarget& window)
{
	Sprite s(LightLayer.getTexture());
	s.setColor(Color(255, 255, 255, 150));
	window.draw(s);
}

//绘制临时图层
void DrawNowLayer(RenderTarget& window)
{
	auto& pd = GetCurPage();
	float scale = pd.Scale / 100.0f;   // 缩放因子

	static bool NeedFlushImage = false;
	if (!WriteDataTemp.empty())
	{
		WriteTempLayer.clear(Color::Transparent);

		//用 SDF 距离场绘制当前笔画：每个像素只算一次到最近线段的距离，等价"无限采样"，
		//且整体只有一次 draw call，与笔画长度无关。
		bool useSDF = SDFLineRenderer::Instance().Ready();

		//SDF 分支需要按"同色同宽"分组提交，这里以第一条笔画的颜色/线宽为基准
		if (useSDF)
		{
			float firstW = WriteDataTemp[0].w * scale;
			SDFBatchBegin(WriteDataTemp[0].color, max(firstW * 0.5f, 0.5f),
				Tool::PenCap == 2 ? (150.f / 255.f) : 1.f);
		}
		else XBatch::begin(WriteTempLayer);

		for (int i = 0; i < WriteDataTemp.size(); i++)
		{
			auto& data = WriteDataTemp[i];

			// 坐标 + 缩放
			float sx1 = (data.x + pd.CameraPos.x) * scale;
			float sy1 = (data.y + pd.CameraPos.y) * scale;
			float sx2 = (data.x2 + pd.CameraPos.x) * scale;
			float sy2 = (data.y2 + pd.CameraPos.y) * scale;

			if (data.StartX < 0 && data.StartY < 0)
			{
				if (useSDF)
				{
					//线宽更新时先冲刷，保证同批次线宽一致
					float halfW = max(data.w * scale * 0.5f, 0.5f);
					if (std::abs(halfW - g_SDFHalfW) > 0.01f)
					{
						SDFBatchFlush(WriteTempLayer);
						SDFBatchBegin(data.color, halfW,
							Tool::PenCap == 2 ? (150.f / 255.f) : 1.f);
					}
					SDFBatchPush(WriteTempLayer, sx1, sy1, sx2, sy2);
				}
				else
				{
					XGraph::LineShape::SetLineWidth(data.w * scale);
					XGraph::SetColor(data.color);
					XGraph::LineShape::Line(sx1, sy1, sx2, sy2, WriteTempLayer);
				}
			}
			else
			{
				//柳叶笔是三角形填充，SDF 线段着色器无法覆盖，始终走三角光栅路径；
				//先冲刷 SDF 批次，绘制完三角后再重开，保证三角形不被 SDF 覆盖
				if (useSDF) SDFBatchFlush(WriteTempLayer);
				else XBatch::end();

				float ssx1 = (data.StartX + pd.CameraPos.x) * scale;
				float ssy1 = (data.StartY + pd.CameraPos.y) * scale;

				DrawFilledPolygon({ {(int)ssx1,(int)ssy1},{(int)sx1,(int)sy1},{(int)sx2,(int)sy2} }, data.color, WriteTempLayer);

				//三角形绘制完成后重开批次，保证后续线段仍能被 SDF 收集
				if (useSDF) SDFBatchBegin(data.color, max(data.w * scale * 0.5f, 0.5f),
					Tool::PenCap == 2 ? (150.f / 255.f) : 1.f);
				else XBatch::begin(WriteTempLayer);
			}

			if(!WriteCamera::EnableWriteCamera)
			{
				if (data.w < Tool::PenSize) data.w += 0.4 * scale;
			}
			else
			{
				if (data.w < Tool::PenSize / 3) data.w += 0.15 * scale;
			}
				
		}

		if (useSDF) SDFBatchFlush(WriteTempLayer);
		else XBatch::end();

		WriteTempLayer.display();

		Sprite s(WriteTempLayer.getTexture());
		if (Tool::PenCap == 2) s.setColor(Color(255, 255, 255, 150));
		else s.setColor(Color(255, 255, 255, 255));
		window.draw(s);

		NeedFlushImage = true;
	}
	// 刷新页面
	else if (NeedFlushImage)
	{
		NeedFlushImage = false;

		Sprite s(WriteTempLayer.getTexture());
		if (Tool::PenCap != 2)
		{
			WriteLayer.draw(s);
			WriteLayer.display();
		}
		else
		{
			LightLayer.draw(s);
			LightLayer.display();
		}

		if (Tool::PenCap != 2)
		{
			s.setTexture(WriteLayer.getTexture()); //避免重复依赖 Temp
			window.draw(s);
		}
		
	}
}

//绘制图片层
static int SkipImageIndex = -1;
void EditImage(int i, RenWin& window);
void DrawImageLayer(RenWin& window)
{

	auto& pd = GetCurPage();

	static Vector2i CamPosTemp;
	if (CamPosTemp != pd.CameraPos || FlushImageLayer)
	{
		float scale = pd.Scale / 100.0f;
		ImageLayer.clear(Color::Transparent);

		//循环不变量提到循环外，避免每轮重复计算
		float camScaleX = pd.CameraPos.x * scale;
		float camScaleY = pd.CameraPos.y * scale;
		int imageCount = (int)pd.Images.size();
		for (int i = 0; i < imageCount; i++)
		{
			if (i == SkipImageIndex) continue;

			auto& img = pd.Images[i];

			float sx = img.pos.x * scale + camScaleX;
			float sy = img.pos.y * scale + camScaleY;
			float sw = img.w * scale;
			float sh = img.h * scale;

			if (sx - sw / 2 < -sw || sy - sh / 2 < -sh || sx - sw / 2 > WindowSize.x || sy - sw / 2 > WindowSize.y) continue;

			float ScaleX = sw / (float)img.img.w;
			float ScaleY = sh / (float)img.img.h;
			XImage::PutRoteScaleImage(img.img, sx, sy,img.rote, ScaleX, ScaleY, ImageLayer, 0.5, 0.5);
		}

		ImageLayer.display();

		CamPosTemp = pd.CameraPos;
		FlushImageLayer = false;
	}

	if(!WriteCamera::EnableWriteCamera)
	{
		if (XMsg::MouseMsg::IsMouseDown(VK::MouseRight))
		{
			float scale = pd.Scale / 100.0f;
			//循环不变量提到循环外
			float camScaleX = pd.CameraPos.x * scale;
			float camScaleY = pd.CameraPos.y * scale;
			int imageCount = (int)pd.Images.size();
			for (int i = 0; i < imageCount; i++)
			{
				auto& img = pd.Images[i];

				float sx = img.pos.x * scale + camScaleX;
				float sy = img.pos.y * scale + camScaleY;
				float sw = img.w * scale;
				float sh = img.h * scale;

				if (sx - sw / 2 < -sw || sy - sh / 2 < -sh || sx > WindowSize.x || sy > WindowSize.y) continue;

				if (XMsg::MouseMsg::IsMouseIn(sx - sw / 2, sy - sh / 2, sw, sh))
				{
					SkipImageIndex = i;
					EditImage(i, window);
					SkipImageIndex = -1;
					break;
				}
			}
		}
	}

	//注意：不能用 static——ImageLayer 在窗口尺寸/设置变化时会被重建，
	//静态 Sprite 会持有已析构的旧纹理。
	Sprite s(ImageLayer.getTexture());
	window.draw(s);
}

#pragma endregion

//图形矫正
#pragma region MyRegion

bool StraightenWriteData(
	std::vector<WriteData>& WriteDataTemp,
	float tolerance = 10.0f * ScreenScale,
	float minLength = 200.f)
{
	size_t N = WriteDataTemp.size();
	if (N <= 1) return false;

	float x1 = WriteDataTemp.front().x;
	float y1 = WriteDataTemp.front().y;
	float x2 = WriteDataTemp.back().x;
	float y2 = WriteDataTemp.back().y;

	float dx = x2 - x1;
	float dy = y2 - y1;
	float segLen = std::hypot(dx, dy);

	if (segLen < minLength) return false;
	if (N <= 4) return false;

	// 检查偏离
	int outOfRangeCount = 0;
	for (size_t k = 1; k < N - 1; ++k) {
		float px = WriteDataTemp[k].x;
		float py = WriteDataTemp[k].y;
		float num = std::abs((y2 - y1) * px - (x2 - x1) * py + x2 * y1 - y2 * x1);
		float den = std::hypot(y2 - y1, x2 - x1);
		float dist = (den > 1e-6f) ? num / den : 0.0f;
		if (dist > tolerance) {
			outOfRangeCount++;
		}
	}

	if (outOfRangeCount > (int)(N - 2) / 4) return false;

	// 两端都投影到首尾直线
	for (size_t k = 0; k < N; ++k) {
		// 投影 (x, y)
		float proj = ((WriteDataTemp[k].x - x1) * dx + (WriteDataTemp[k].y - y1) * dy)
			/ (dx * dx + dy * dy + 1e-6f);
		WriteDataTemp[k].x = x1 + proj * dx;
		WriteDataTemp[k].y = y1 + proj * dy;

		// 投影 (x2, y2)
		float proj2 = ((WriteDataTemp[k].x2 - x1) * dx + (WriteDataTemp[k].y2 - y1) * dy)
			/ (dx * dx + dy * dy + 1e-6f);
		WriteDataTemp[k].x2 = x1 + proj2 * dx;
		WriteDataTemp[k].y2 = y1 + proj2 * dy;
	}

	return true;
}

struct LineEndpoints
{
	float x1, y1;
	float x2, y2;
};

LineEndpoints GetStraightenedEndpoints(
	const std::vector<WriteData>& WriteDataTemp,
	float scale,
	const Vector2i& camPos)
{
	LineEndpoints ep;
	size_t N = WriteDataTemp.size();
	if (N == 0) return {};

	float camScaleX = camPos.x * scale;
	float camScaleY = camPos.y * scale;

	// 起点（用第一个点的 x/y）
	ep.x1 = WriteDataTemp.front().x * scale + camScaleX;
	ep.y1 = WriteDataTemp.front().y * scale + camScaleY;

	// 终点（用最后一个点的 x2/y2，和你渲染逻辑一致）
	ep.x2 = WriteDataTemp.back().x2 * scale + camScaleX;
	ep.y2 = WriteDataTemp.back().y2 * scale + camScaleY;

	return ep;
}

bool StraightenWriteDataToCircle(
	std::vector<WriteData>& WriteDataTemp,
	float toleranceRatio = 0.18f,
	float minRadius = 60.f * ScreenScale,
	float closeRatio = 0.85f)
{
	size_t N = WriteDataTemp.size();
	if (N <= 4) return false;

	// 取中点进行圆拟合 (抗噪)
	std::vector<float> px(N), py(N);
	for (size_t i = 0; i < N; ++i) {
		px[i] = (WriteDataTemp[i].x + WriteDataTemp[i].x2) * 0.5f;
		py[i] = (WriteDataTemp[i].y + WriteDataTemp[i].y2) * 0.5f;
	}

	float mx = 0.f, my = 0.f;
	for (size_t i = 0; i < N; ++i) { mx += px[i]; my += py[i]; }
	mx /= N; my /= N;

	// Taubin 圆拟合
	float Suu = 0.f, Svv = 0.f, Suuv = 0.f, Suvv = 0.f, Suuu = 0.f, Svvv = 0.f;
	for (size_t i = 0; i < N; ++i) {
		float u = px[i] - mx, v = py[i] - my;
		float u2 = u * u, v2 = v * v;
		Suu += u2; Svv += v2; Suuv += u2 * v; Suvv += u * v2; Suuu += u2 * u; Svvv += v2 * v;
	}

	float A = Suu + Svv;
	if (std::abs(A) < 1e-6f) return false;
	float B = 0.5f * (Suuu + Suuv + Suvv + Svvv);
	float cx = mx + (Suuv * A - Suvv * B) / (A * A + 1e-6f);
	float cy = my + (Suvv * A - Suu * B) / (A * A + 1e-6f);

	// 计算平均半径
	float meanR = 0.f;
	for (size_t i = 0; i < N; ++i) meanR += std::sqrt((px[i] - cx) * (px[i] - cx) + (py[i] - cy) * (py[i] - cy));
	meanR /= N;

	if (meanR < minRadius) return false;

	// 容错检查
	float tol = meanR * toleranceRatio;
	int outliers = 0;
	for (size_t i = 0; i < N; ++i) {
		float r = std::sqrt((px[i] - cx) * (px[i] - cx) + (py[i] - cy) * (py[i] - cy));
		if (std::abs(r - meanR) > tol) ++outliers;
	}
	// 离群点比例上限从 25% 收紧到 12%：椭圆各点半径差异大，会频繁超出收紧后的容差，
	// 从而被判为离群，避免椭圆被强行拉成圆。
	if (outliers > (int)(N * 0.12f)) return false;

	// 角度覆盖检查：逐段累加相邻点的角度增量，得到真实扫过的总弧度。
	// 不能用 atan2(尾) - atan2(首) 直接相减——那样会把"绕了多圈"的信息丢掉，
	// 只要首尾角度接近就误判为闭合圆（例如只画了 1/4 圈的弧，起止点却靠得很近）。
	float sweep = 0.f;
	for (size_t i = 1; i < N; ++i) {
		float a0 = std::atan2(py[i - 1] - cy, px[i - 1] - cx);
		float a1 = std::atan2(py[i] - cy, px[i] - cx);
		float delta = a1 - a0;

		// 归一化到 (-pi, pi]，避免跨越 ±pi 时出现 2pi 的假跳变
		while (delta > 3.14159265f) delta -= 2.f * 3.14159265f;
		while (delta < -3.14159265f) delta += 2.f * 3.14159265f;

		sweep += delta;
	}

	// 累计增量可能为负（顺时针绘制），取绝对值即为实际扫过角度
	float swept = std::abs(sweep);

	// 采样过疏时，相邻点之间可能存在大跨度，导致扇形角丢失；
	// 这里用"首尾直线距离 vs 弧长"做一次粗校验，防止窄扇形被当成整圆。
	// 若首尾很近但累计角度很大，说明确实绕行了多圈，此时保留判定。
	if (swept < 2.f * 3.14159265f * closeRatio) return false;

	// 【核心】独立投影 + 首尾微调
	for (size_t i = 0; i < N; ++i) {
		// 1. 内点 (x,y) 投影到平均半径
		float ang1 = std::atan2(WriteDataTemp[i].y - cy, WriteDataTemp[i].x - cx);
		WriteDataTemp[i].x = cx + meanR * std::cos(ang1);
		WriteDataTemp[i].y = cy + meanR * std::sin(ang1);

		// 2. 外点 (x2,y2) 独立投影到平均半径 (不再人为加宽，消除交错)
		float ang2 = std::atan2(WriteDataTemp[i].y2 - cy, WriteDataTemp[i].x2 - cx);
		WriteDataTemp[i].x2 = cx + meanR * std::cos(ang2);
		WriteDataTemp[i].y2 = cy + meanR * std::sin(ang2);

		// 3. 处理极短线段（若原x2y2与xy几乎重合，保持重合避免单点断裂）
		float origLen = std::hypot(WriteDataTemp[i].x2 - WriteDataTemp[i].x, WriteDataTemp[i].y2 - WriteDataTemp[i].y);
		if (origLen < 1e-4f) {
			WriteDataTemp[i].x2 = WriteDataTemp[i].x;
			WriteDataTemp[i].y2 = WriteDataTemp[i].y;
		}
	}

	// 4. 首尾闭合微调（解决图中左侧微缺口）
	// 追加约束：首尾间距需小于 0.05 倍半径，超过则视为开口弧（非闭合圆），不做拉齐，
	// 避免把用户有意留出的缺口强行闭合。
	if (N > 1) {
		float gap = std::hypot(WriteDataTemp[0].x - WriteDataTemp[N - 1].x, WriteDataTemp[0].y - WriteDataTemp[N - 1].y);
		if (gap <= meanR * 0.2f) { // 首尾间距不大于5%半径才拉齐
			float midX = (WriteDataTemp[0].x + WriteDataTemp[N - 1].x) * 0.5f;
			float midY = (WriteDataTemp[0].y + WriteDataTemp[N - 1].y) * 0.5f;
			WriteDataTemp[0].x = WriteDataTemp[N - 1].x = midX;
			WriteDataTemp[0].y = WriteDataTemp[N - 1].y = midY;

			// 同步x2y2的闭合
			float midX2 = (WriteDataTemp[0].x2 + WriteDataTemp[N - 1].x2) * 0.5f;
			float midY2 = (WriteDataTemp[0].y2 + WriteDataTemp[N - 1].y2) * 0.5f;
			WriteDataTemp[0].x2 = WriteDataTemp[N - 1].x2 = midX2;
			WriteDataTemp[0].y2 = WriteDataTemp[N - 1].y2 = midY2;
		}
	}

	return true;
}


#pragma endregion

//对齐线
#pragma region MyRegion

struct AdjustLineS
{
	int x = 0, y = 0, a = 0;
	EV l;
	int time = 0;
	int targetLen = 0;

	bool enabled = false;
};
AdjustLineS AdjustLineLine;

void DrawAdjustLine(RenWin& window)
{
	if (!AdjustLineLine.enabled)
	{
		AdjustLineLine.a = 0;
		AdjustLineLine.time = 0;
		return;
	}

	if (AdjustLineLine.time > 0)
	{
		AdjustLineLine.time -= 1;
		AdjustLineLine.a = min(255, AdjustLineLine.a + 10);
	}
	else
	{
		AdjustLineLine.a = max(0, AdjustLineLine.a - 10);
		if (AdjustLineLine.a == 0)
		{
			AdjustLineLine.x = AdjustLineLine.y = 0;
			AdjustLineLine.targetLen = 0;
		}
	}

	if (AdjustLineLine.a > 0)
		AdjustLineLine.l.UpdateAnimation(XEase::EaseBasic::easeOut, 4);

	if (AdjustLineLine.x <= 0 || AdjustLineLine.y <= 0 || AdjustLineLine.a <= 0 || AdjustLineLine.l.value <= 0) return;

	XGraph::SetColor(Color(255, 255, 255, AdjustLineLine.a * 0.05));
	static int lw = max(1, WindowSize.x / 400);
	XGraph::LineShape::SetLineWidth(lw);
	XGraph::LineShape::Line(
		AdjustLineLine.x,
		AdjustLineLine.y,
		AdjustLineLine.x + AdjustLineLine.l.value,
		AdjustLineLine.y,
		window
	);
}

static int LineStartX = -1;
static int LineStartY = -1;
static int LineBaseY = -1;
static int LastPosY = -1;
static bool LineBaseLocked = false; //基线是否锁定

void AdjustLineManager(Vector2i& pos)
{
	const int Y_THRESHOLD = WindowSize.y / 40;
	const int BASE_OFFSET = WindowSize.y / 120;

	// 首次或已消失后重新落笔
	if (AdjustLineLine.a <= 0)
	{
		LineStartX = pos.x;
		LineStartY = pos.y;
		LineBaseY = pos.y;
		LastPosY = pos.y;
		LineBaseLocked = false; //  新行开始，未锁定

		AdjustLineLine.x = LineStartX;
		AdjustLineLine.y = LineBaseY + BASE_OFFSET;
		AdjustLineLine.a = 255;
		AdjustLineLine.time = 120;
		AdjustLineLine.targetLen = 0;
		AdjustLineLine.l.SetAnimationStartValue(0);
		return;
	}

	// 换行
	if (pos.y - LastPosY > Y_THRESHOLD)
	{
		LineStartX = pos.x;
		LineStartY = pos.y;
		LineBaseY = pos.y;
		LastPosY = pos.y;
		LineBaseLocked = false; // 换行重新锁定

		AdjustLineLine.x = LineStartX;
		AdjustLineLine.y = LineBaseY + BASE_OFFSET;
		AdjustLineLine.a = 0;
		AdjustLineLine.time = 0;
		AdjustLineLine.l.SetAnimationStartValue(0);
		return;
	}

	// 只在未锁定时跟踪最低点
	if (!LineBaseLocked)
	{
		if (pos.y > LineBaseY)
			LineBaseY = pos.y;

		// 横向移动超过一个阈值，认为第一个字已经落下，锁定基线

		static int NEXTFONT = WindowSize.x / 15;
		if (pos.x - LineStartX > NEXTFONT)
			LineBaseLocked = true;
	}

	if (pos.y > LastPosY) LastPosY = pos.y;

	// 同一行只更新长度
	int curLen = pos.x - LineStartX;
	if (curLen > 0)
	{
		static int AddLengh = WindowSize.x / 15;

		if(AdjustLineLine.targetLen < curLen + AddLengh)
		{
			AdjustLineLine.targetLen = curLen + AddLengh;
			AdjustLineLine.l.SetAnimation(AdjustLineLine.targetLen, 30);
			AdjustLineLine.time = 120;
		}
	}

	// 线始终贴在锁定后的基线
	AdjustLineLine.y = LineBaseY + BASE_OFFSET;
}

#pragma endregion

//Tool回收检测
#pragma region MyRegion

void ToolExpCheck(Vector2i& Pos)
{
	//Tool回收检测

	static int BarX1 = WindowSize.x * 0.2,BarX2 = WindowSize.x * 0.8,BarY = WindowSize.y * 0.75;
	if (Pos.y > BarY)
	{
		if (Pos.x <= BarX1)
		{
			Tool::Exp::ExpMinBar();
			Tool::Exp::ExpStartBar();
		}
		else if (Pos.x >= BarX2)
		{
			Tool::Exp::ExpPageBar();
		}
		else Tool::Exp::ExpBottomToolBar();
	}

	static int BarY1 = WindowSize.y * 0.4, BarY2 = WindowSize.y * 0.6;
	if (Pos.y > BarY1 && Pos.y < BarY2)
	{
		if (Pos.x <= BarX1) Tool::Exp::ExpLeftScrollBar();
		if (Pos.x >= BarX2) Tool::Exp::ExpRightScrollBar();
	}
}

#pragma endregion

//书写子系统
void WriteSub(RenWin& window)
{
	//柳叶笔记录点
	static Vector2i LeafPenStartPoint = { -1,-1 };

	static float PenW;

	// 上一次书写点，默认-1，-1
	static Vector2i LastPoint = { -1,-1 };
	static int WriteTime = 0;

	auto& pd = GetCurPage();
	float scale = pd.Scale / 100.0f;   // 缩放因子

	if (WriteStopTime > 0)
	{
		WriteStopTime -= 1;
		return;
	}

	if (Tool::ToolCount != 0 || MoveSpeed.x != 0 || MoveSpeed.y != 0) return;

	Vector2i WritePos = XMsg::TouchMsg::GetTouchPos();

	// 当触控点为1时才执行
	if (IsTouching()
		&& !Tool::IsInBar && TouchNum < 2)
	{
		// 触控落在思维导图的 + 按钮 / 调色按钮上时，跳过落笔：
		// 否则点击按钮会穿透到底层画布，在按钮位置留下多余笔画，
		// 该短笔画还会让所属节点的底线被误判为「有内容」而错误延展。
		// 命中范围同时包含 + / 调色 / 删除(-) 三个按钮。
		if (MindMap::IsBtnHit())
		{
			// 关键：清空本帧已累积的按钮触控笔迹，防止它被提交成真实笔画
			WriteDataTemp.clear();
			AdjustLineLine.enabled = false;

			// 同步重置书写参考点，避免按钮点击结束后与后续笔画连成一条线
			LastPoint.x = LastPoint.y = -1;
			LeafPenStartPoint.x = LeafPenStartPoint.y = -1;
			return;
		}

		if(WritePos != LastPoint || WriteDataTemp.empty())
		{
			if (WritePos.x < 1 || WritePos.y < 1) return;

			WriteData NewData;

			// 获取坐标
			if (LastPoint.x < 0 || LastPoint.y < 0)
			{
				LastPoint = WritePos;

				static int XOffset = -WindowSize.x / 800;
				static int YOffset = XOffset;

				LastPoint.x += XOffset;
				LastPoint.y += YOffset;
			}

			// 坐标除以缩放，保证存入的是世界坐标
			NewData.x = LastPoint.x / scale - pd.CameraPos.x;
			NewData.y = LastPoint.y / scale - pd.CameraPos.y;
			NewData.x2 = WritePos.x / scale - pd.CameraPos.x;
			NewData.y2 = WritePos.y / scale - pd.CameraPos.y;

			if (PenW > Tool::PenSize / 2 && Tool::PenCap == 0)
			{				
				if (!WriteCamera::EnableWriteCamera) PenW -= 0.4 * GetCurPage().Scale / 100.0;
				else PenW -= 0.2 * GetCurPage().Scale / 100.0;
			}

			// 线宽按缩放比例存
			if (Tool::PenCap == 2)
			{
				if(!WriteCamera::EnableWriteCamera) NewData.w = PenW * 2;
				else NewData.w = PenW * 2 / 3;
			}
			else 
			{
				if (!WriteCamera::EnableWriteCamera) NewData.w = PenW;
				else NewData.w = PenW / 3;
			}

			//柳叶笔
			if (Tool::PenCap == 3)
			{
				if (LeafPenStartPoint.x < 0 || LeafPenStartPoint.y < 0)
				{
					LeafPenStartPoint = WritePos;
				}

				NewData.StartX = LeafPenStartPoint.x / scale - pd.CameraPos.x;
				NewData.StartY = LeafPenStartPoint.y / scale - pd.CameraPos.y;
			}

			NewData.color = Tool::PenColor;
			if (Tool::PenCap == 2) NewData.w *= 1.5;
			NewData.Light = Tool::PenCap == 2;

			NewData.TempLayer = TempLayer;

			CameraManager::SleepUpdate(120);

			AdjustLineManager(WritePos);

			WriteDataTemp.push_back(NewData);

			static int MAX_ADJUSTlINE_SHOW_BLOCK = 35 * ScreenScale;
			if (WriteDataTemp.size() > MAX_ADJUSTlINE_SHOW_BLOCK) AdjustLineLine.enabled = false;

			if (WriteTime < 30) WriteTime += 1;
		}

		ToolExpCheck(WritePos);

		LastPoint = WritePos;
	}
	// 非触控
	else
	{
		LastPoint.x = LastPoint.y = -1;
		LeafPenStartPoint.x = LeafPenStartPoint.y = -1;

		// 写字判定
		if (User::EnableWriteAdjust)
		{
			if (!WriteDataTemp.empty())
			{
				int minX = INT_MAX, maxX = INT_MIN;
				int minY = INT_MAX, maxY = INT_MIN;

				for (auto& d : WriteDataTemp)
				{
					minX = min(minX, min(d.x, d.x2));
					maxX = max(maxX, max(d.x, d.x2));
					minY = min(minY, min(d.y, d.y2));
					maxY = max(maxY, max(d.y, d.y2));
				}

				int dx = maxX - minX;
				int dy = maxY - minY;
				int maxDist = max(dx, dy);

				//  写字特征：单笔位移不大（画图/涂鸦会很大）
				static const int MAX_WRITE_DIST = WindowSize.x / 13;
				AdjustLineLine.enabled = (maxDist < MAX_WRITE_DIST);
			}
		}
		else AdjustLineLine.enabled = false;
		//caonimade

		PenW = Tool::PenSize;

		// 清理书写缓存
		if (!WriteDataTemp.empty())
		{
			auto& page = GetCurPage();
			auto& data = page.Data;

			if (data.empty())
				data.emplace_back();

			bool newRowCreated = false;
			if (data.back().size() + WriteDataTemp.size() > MAX_MEMORY_BLOCK)
			{
				data.emplace_back();
				newRowCreated = true;
			}

			page.UnDoData.push({
				static_cast<int>(data.size() - 1),
				newRowCreated ? std::vector<WriteData>{} : data.back()
				});

			if(User::EnableFixShape)
			{
				if (StraightenWriteDataToCircle(WriteDataTemp))
				{
					DrawNowLayer(window);
				}
				if (StraightenWriteData(WriteDataTemp))
				{
					LineEndpoints Straightens = GetStraightenedEndpoints(WriteDataTemp, scale, pd.CameraPos);

					Color color = XColor::LightColor(Tool::PenColor, 0.7);
					if (Tool::PenColor == Color::White) color = User::MainColor;
					
					AddPer(Straightens.x1, Straightens.y1, Straightens.x2, Straightens.y2, color);

					DrawNowLayer(window);
				}
			}

			data.back().insert(
				data.back().end(),
				std::make_move_iterator(WriteDataTemp.begin()),
				std::make_move_iterator(WriteDataTemp.end())
			);

			WriteDataTemp.clear();
		}

		WriteTime = 0;
	}
}

//移动/缩放相机
void MoveCamera()
{
	static int MaxMoveSpeed = 30;

	//归位
	ResetPos.x.UpdateAnimation(XEase::EaseBasic::easeInOut, 4);
	ResetPos.y.UpdateAnimation(XEase::EaseBasic::easeInOut, 4);
	if (ResetPos.x.IsAnimation()) GetCurPage().CameraPos.x = ResetPos.x.value;
	if (ResetPos.y.IsAnimation()) GetCurPage().CameraPos.y = ResetPos.y.value;

	static const int CameraTo = 1;

	int x, y;
	Vector2i mousePos = XMsg::TouchMsg::GetTouchPos();
	x = mousePos.x;
	y = mousePos.y;

	static int TouchTime = 0;

	static int MoveSleepTime = 0;
	static int onef = 0;

	//单指冷却
	if (onef > 0) onef -= 1;

	static Vector2i temp_mousep;

	//触点数量变化（双指先后按下/松开存在时间差）时，参考点必须复位：
	//否则会把"触点数的跳变"误当成一次真实拖动，导致画面瞬移（跑）。
	static int LastTouchNum = 0;
	if (TouchNum != LastTouchNum)
	{
		LastTouchNum = TouchNum;
		temp_mousep.x = temp_mousep.y = 0;
		MoveSpeed = { 0, 0 };
	}

	//移动
	if ((Tool::ToolCount == 2 && (TouchNum == 1 || XMsg::KeyMsg::Keystate(VK::MouseLeft))) || (Tool::ToolCount != 2 && TouchNum >= 2 && TouchNum <= 5))
	{
		WriteStopTime = 15;
		if (MoveSleepTime == 0)
		{
			if (TouchNum == 1)
			{
				if (TouchTime < 60)TouchTime += 1;
			}

			if (temp_mousep.x != x || temp_mousep.y != y)
			{
				TouchTime = 60;
			}
			if ((TouchNum >= 2 && TouchTime > 10) || (onef == 0 && TouchTime > 10 && TouchNum == 1) || (XMsg::KeyMsg::Keystate(VK::MouseLeft) && TouchNum == 0))
			{
				if (temp_mousep.x > 0 && temp_mousep.y > 0)
				{
					if (x - temp_mousep.x != 0 && y - temp_mousep.y != 0)
					{
						int movex = 0, movey = 0;
						if (movex < MaxMoveSpeed * ScreenScale) movex = x - temp_mousep.x;
						if (movey < MaxMoveSpeed * ScreenScale) movey = y - temp_mousep.y;
						MoveSpeed = { movex,movey };
					}
					temp_mousep.x = x;
					temp_mousep.y = y;
				}
				else
				{
					temp_mousep.x = x;
					temp_mousep.y = y;
				}
			}

			if (TouchNum >= 2 && onef < 15) onef = 15;
		}
	}
	else
	{
		if (MoveSleepTime > 0) MoveSleepTime -= 1;

		temp_mousep.x = temp_mousep.y = 0;
		TouchTime = 0;
	}

	//缩放
	if (Tool::ToolCount == 2 || (TouchNum >= 2 && TouchNum <= 5))
	{
		int wheel = XMsg::MouseMsg::GetMouseWheel();

		if (wheel != 0)
		{
			POINT mp = { x,y };

			int prevScale = GetCurPage().Scale;
			int delta = wheel / 100 / ScreenScale;

			MoveSleepTime = 10;
			onef = 10;

			int AfterScale = prevScale + delta;
			if (AfterScale < 5) AfterScale = 5;
			if (AfterScale > 5000) AfterScale = 5000;

			// 只有在实际缩放发生变化时才调整位置
			if (AfterScale != prevScale)
			{
				auto& pd = GetCurPage();

				float oldScale = pd.Scale / 100.0f;
				float newScale = AfterScale / 100.0f;

				float cx = WindowSize.x / 2.0f;
				float cy = WindowSize.y / 2.0f;

				pd.CameraPos.x += (int)(cx * (1.0f / newScale - 1.0f / oldScale));
				pd.CameraPos.y += (int)(cy * (1.0f / newScale - 1.0f / oldScale));

				pd.Scale = AfterScale;

				GetCurPage().Scale = AfterScale;

				if (BottomMessage::IsMessageNow(101))
				{
					BottomMessage::UpdateCurrentMessageText(L"缩放: " + to_wstring(AfterScale) + L"%");
					BottomMessage::UpdateCurretnMessageTime(120);
				}
				else
				{
					BottomMessage::AddMessage(101, L"缩放: " + to_wstring(AfterScale) + L"%", BottomMessageType_INFO,120);
				}

				FlushWriteLayer = true;
			}
		}
	}
	//
	//if (MoveSpeed.x > MaxMoveSpeed * ScreenScale)MoveSpeed.x = MaxMoveSpeed * ScreenScale;
	//if (MoveSpeed.y > MaxMoveSpeed * ScreenScale)MoveSpeed.y = MaxMoveSpeed * ScreenScale;

	if (MoveSpeed.x != 0 || MoveSpeed.y != 0)
	{
		if (MoveSpeed.x - CameraTo > 0) MoveSpeed.x -= CameraTo;
		else if (MoveSpeed.x + CameraTo < 0) MoveSpeed.x += CameraTo;
		else MoveSpeed.x = 0;
		if (MoveSpeed.y - CameraTo > 0) MoveSpeed.y -= CameraTo;
		else if (MoveSpeed.y + CameraTo < 0) MoveSpeed.y += CameraTo;
		else MoveSpeed.y = 0;

		auto& pd = GetCurPage();
		float scale = pd.Scale / 100.0f;

		// 关键：屏幕像素速度 -> 世界坐标位移
		pd.CameraPos.x += (int)(MoveSpeed.x / scale);
		pd.CameraPos.y += (int)(MoveSpeed.y / scale);

		FlushWriteLayer = true;
	}

	if (WriteCamera::EnableWriteCamera)
	{
		// 与 WriteCamera::Draw 保持一致：画面中心在屏幕上的位置为
		// (CameraPos + 窗口半宽) * Scale；且画面被旋转 -90°，
		// 因此屏幕上的水平尺寸对应采集帧的高、垂直尺寸对应采集帧的宽。
		float CameraScale = GetCurPage().Scale / 100.0f;
		if (CameraScale < 0.01f) CameraScale = 0.01f; // 防止极端缩放导致除零

		float halfWinX = WindowSize.x / 2.0f;
		float halfWinY = WindowSize.y / 2.0f;

		// 画面在屏幕上的实际尺寸（旋转 -90° 后宽高互换）
		float camScreenW;
		float camScreenH;
		

		if(CameraManager::Rotation.value == -90 || CameraManager::Rotation.value == -270)
		{
			camScreenW = CameraManager::GetH() * CameraScale;
			camScreenH = CameraManager::GetW() * CameraScale;
		}
		else
		{
			camScreenW = CameraManager::GetW() * CameraScale;
			camScreenH = CameraManager::GetH() * CameraScale;
		}

		// 保证最大位移时至少有 0.1 比例的尺寸仍留在窗口内：
		// 画面中心越过窗口边缘 0.4 * 屏幕尺寸时，窗口内恰好只剩 0.1。
		float cxMin = -0.4f * camScreenW;
		float cxMax = WindowSize.x + 0.4f * camScreenW;
		float cyMin = -0.4f * camScreenH;
		float cyMax = WindowSize.y + 0.4f * camScreenH;

		// 由屏幕中心反推 CameraPos：CameraPos = 屏幕中心 / Scale - 窗口半宽
		Clamp(GetCurPage().CameraPos.x,
			(int)(cxMin / CameraScale - halfWinX),
			(int)(cxMax / CameraScale - halfWinX));
		Clamp(GetCurPage().CameraPos.y,
			(int)(cyMin / CameraScale - halfWinY),
			(int)(cyMax / CameraScale - halfWinY));
	}
}

//书写显示
int FrontAlpha = 0;
void Write::Show(RenWin& window)
{
	TouchNum = XMsg::TouchMsg::GetTouchNum();

	//初始化
	static bool DataInit = false;
	if (!DataInit)
	{
		sf::Vector2u rtSize(
			static_cast<unsigned int>(WindowSize.x),
			static_cast<unsigned int>(WindowSize.y)
		);

		WriteTempLayer = RenderTexture(rtSize);

		WriteLayer = RenderTexture(rtSize);
		LightLayer = RenderTexture(rtSize);
		ImageLayer = RenderTexture(rtSize);

		WriteTempLayer.clear(Color::Transparent);
		WriteLayer.clear(Color::Transparent);
		LightLayer.clear(Color::Transparent);
		ImageLayer.clear(Color::Transparent);

		EraseStf = WindowSize.x * ScreenScale / 1400.0;

		TotalPage = 1;

		DataInit = true;
	}

	//设置更改
	if (UpdateUser > 0)
	{
		sf::Vector2u rtSize(
			static_cast<unsigned int>(WindowSize.x),
			static_cast<unsigned int>(WindowSize.y)
		);

		WriteTempLayer = RenderTexture(rtSize);

		WriteLayer = RenderTexture(rtSize);
		LightLayer = RenderTexture(rtSize);
		ImageLayer = RenderTexture(rtSize);

		WriteTempLayer.clear(Color::Transparent);
		WriteLayer.clear(Color::Transparent);
		LightLayer.clear(Color::Transparent);
		ImageLayer.clear(Color::Transparent);

		FlushWriteLayer = true;
	}

	//绘制展台
	WriteCamera::Draw(window);

	//绘制图片
	DrawImageLayer(window);

	//绘制之前书写
	DrawLayer(window);

	//绘制当前书写
	DrawNowLayer(window);

	//绘制荧光层
	DrawLightLayer(window);

	//绘制橡皮擦
	DrawEraseRect(window);

	//更新思维导图（按钮命中、节点选中、底线拓展）
	MindMap::Update();

	//处理思维导图的调色请求：点击颜色按钮后弹出色盘并写回节点颜色。
	// 放在这里是因为 ChooseColorWindow 需要 RenderWindow&，而 Update 没有窗口参数。
	{
		int colorReq = MindMap::TakeColorRequest();
		if (colorReq >= 0)
		{
			int ci = MindMapIndexOf(colorReq);
			if (ci >= 0)
			{
				Color picked = ChooseColorWindow(MindMapNodes[ci].color, window);
				// 改色需连带整棵子树：父项换色后子项继承同一配色，保持导图视觉统一。
				// 这里收集「自身 + 全部后代」的编号后统一写回，而非只改当前节点。
				vector<int> colorIds;
				MindMapCollectSubtree(colorReq, colorIds);
				for (int id : colorIds)
				{
					int i = MindMapIndexOf(id);
					if (i >= 0) MindMapNodes[i].color = picked;
				}
			}
		}
	}

	//绘制思维导图（底线、连接线、+ 按钮）
	MindMap::Draw(window);

	//绘制对齐线
	DrawAdjustLine(window);

	//绘制特效
	DrawPer(window);

	if (FrontAlpha > 0)
	{
		XGraph::SetFillColor(Color(30, 30, 30, FrontAlpha));
		XGraph::RectangleShape::FillRect_WithoutBorder(0, 0, WindowSize.x, WindowSize.y, window);

		if (FrontAlpha - 15 > 0) FrontAlpha -= 15;
		else FrontAlpha = 0;
	}
}

//书写系统
void Write::WriteT(RenWin& window)
{
	MoveCamera();

	//获取擦除状态
	IsErasing = IsErase();
	if (!IsErasing)
	{
		//重置撤销栈标志
		NeedPushUnDoStack = true;
	}
	else
	{
		CameraManager::SleepUpdate(120);
	}

	WriteSub(window);
}

//新建板书
void Write::MakeNewPage()
{
	PageData.emplace_back();
	Page = PageData.size() - 1;
	TotalPage += 1;
}

//下一页/加页
void Write::NextPage()
{
	if(!WriteCamera::EnableWriteCamera)
	{
		if (Page >= TotalPage) Page = TotalPage - 1;

		if (Page == TotalPage - 1)
		{
			PageData.emplace_back();
			Page += 1;
			TotalPage += 1;
		}
		else
		{
			Page += 1;
		}

		if (BottomMessage::IsMessageNow(105))
		{
			BottomMessage::UpdateCurrentMessageText(L"第" + to_wstring(Page + 1) + L"页");
			BottomMessage::ResetY();
			BottomMessage::UpdateCurretnMessageTime(180);
		}
		else
		{
			BottomMessage::AddMessage(105, L"第" + to_wstring(Page + 1) + L"页", BottomMessageType_INFO, 180);
		}

		FrontAlpha = 255;
	}
	else
	{
		if (Page < TotalPage - 1)
		{
			Page += 1;
			FrontAlpha = 255;

			if (BottomMessage::IsMessageNow(105))
			{
				BottomMessage::UpdateCurrentMessageText(L"第" + to_wstring(Page) + L"张");
				BottomMessage::ResetY();
				BottomMessage::UpdateCurretnMessageTime(180);
			}
			else
			{
				BottomMessage::AddMessage(105, L"第" + to_wstring(Page) + L"张", BottomMessageType_INFO, 180);
			}

			Tool::UpdateCameraControl();
		}
	}

	FlushImageLayer = FlushWriteLayer = true;
	
}

//上一页
void Write::LastPage()
{
	if(!WriteCamera::EnableWriteCamera)
	{
		if (Page > 0)
		{
			Page -= 1;

			if (BottomMessage::IsMessageNow(105))
			{
				BottomMessage::UpdateCurrentMessageText(L"第" + to_wstring(Page + 1) + L"页");
				BottomMessage::ResetY();
				BottomMessage::UpdateCurretnMessageTime(180);
			}
			else
			{
				BottomMessage::AddMessage(105, L"第" + to_wstring(Page + 1) + L"页", BottomMessageType_INFO, 180);
			}
			FrontAlpha = 255;
			FlushImageLayer = FlushWriteLayer = true;
		}
		if (Page < 0) Page = 0;
	}
	else
	{
		if (Page > 1)
		{
			Page -= 1;
			FrontAlpha = 255;
			FlushImageLayer = FlushWriteLayer = true;

			if (BottomMessage::IsMessageNow(105))
			{
				BottomMessage::UpdateCurrentMessageText(L"第" + to_wstring(Page) + L"张");
				BottomMessage::ResetY();
				BottomMessage::UpdateCurretnMessageTime(180);
			}
			else
			{
				BottomMessage::AddMessage(105, L"第" + to_wstring(Page) + L"张", BottomMessageType_INFO, 180);
			}

			Tool::UpdateCameraControl();
		}
	}
}

//跳转到指定页
void Write::GoToPage(int page)
{
	if (TotalPage < 1) return;

	if (page < 0) page = 0;
	if (page > TotalPage - 1) page = TotalPage - 1;

	if (page == Page) return;

	Page = page;

	if (BottomMessage::IsMessageNow(105))
	{
		BottomMessage::UpdateCurrentMessageText(L"第" + to_wstring(Page + 1) + L"页");
		BottomMessage::ResetY();
		BottomMessage::UpdateCurretnMessageTime(180);
	}
	else
	{
		BottomMessage::AddMessage(105, L"第" + to_wstring(Page + 1) + L"页", BottomMessageType_INFO, 180);
	}

	FrontAlpha = 255;
	FlushImageLayer = FlushWriteLayer = true;
}

//渲染指定页缩略图到纹理（不改变当前页）
void Write::GetPageThumbnail(int pageIndex, RenderTexture& rt)
{
	if (pageIndex < 0 || pageIndex >= (int)PageData.size()) return;

	//保存当前页
	int oldPage = Page;

	auto& pd = PageData[pageIndex];

	rt.clear(Color(30, 30, 30));

	float scale = pd.Scale / 100.0f;
	if (scale <= 0) scale = 1.0f;

	float camScaleX = pd.CameraPos.x * scale;
	float camScaleY = pd.CameraPos.y * scale;

	//绘制该页所有笔画
	for (int i = 0; i < (int)pd.Data.size(); ++i)
	{
		for (int k = 0; k < (int)pd.Data[i].size(); ++k)
		{
			auto& data = pd.Data[i][k];

			float sx1 = data.x * scale + camScaleX;
			float sy1 = data.y * scale + camScaleY;
			float sx2 = data.x2 * scale + camScaleX;
			float sy2 = data.y2 * scale + camScaleY;

			XGraph::LineShape::SetLineWidth(data.w * scale);
			if (data.StartX < 0 && data.StartY < 0)
			{
				XGraph::SetColor(data.color);
				XGraph::LineShape::Line(sx1, sy1, sx2, sy2, rt);
			}
			else
			{
				float ssx1 = data.StartX * scale + camScaleX;
				float ssy1 = data.StartY * scale + camScaleY;
				DrawFilledPolygon({ {(int)ssx1,(int)ssy1},{(int)sx1,(int)sy1},{(int)sx2,(int)sy2} }, data.color, rt);
			}
		}
	}

	rt.display();

	//恢复当前页（本函数不修改 Page，仅作保险）
	Page = oldPage;
}

//撤销
void Write::UnDo()

{
	auto& page = GetCurPage();
	auto& undoStack = page.UnDoData;
	auto& data = page.Data;

	if (undoStack.empty())
		return;

	auto node = std::move(undoStack.top());
	undoStack.pop();

	// 防御：索引非法
	if (node.Index < 0 || node.Index >= static_cast<int>(data.size()))
		return;

	// 新增行导致的撤销
	if (node.UnDoData.empty())
	{
		data.erase(data.begin() + node.Index);
	}
	// 修改行导致的撤销
	else
	{
		data[node.Index] = std::move(node.UnDoData);
	}

	FlushWriteLayer = true;
}

//擦除全部
void Write::EraseAll()
{
	//推入撤销栈
	if (!GetCurPage().Data.empty() && !GetCurPage().Data.back().empty())
	{
		GetCurPage().UnDoData.push({ static_cast<int>(GetCurPage().Data.size() - 1), GetCurPage().Data.back() });
	}

	GetCurPage().Data.clear();

	GetCurPage().Data.emplace_back();

	// 清屏同步清除思维导图：重置为未激活状态，
	// 避免清屏后仍残留思维导图（与"清屏"语义保持一致）。
	MindMap::Clear();

	FlushWriteLayer = true;
}

//启用/禁用 草稿层
void Write::EnableTempLayer(bool enable)
{
	TempLayer = enable;

	wstring text = enable ? L"打开" : L"关闭";
	text += L"草稿层";

	if (BottomMessage::IsMessageNow(103))
	{
		BottomMessage::UpdateCurrentMessageText(text);
		BottomMessage::UpdateCurretnMessageTime(180);
		BottomMessage::ResetY();
	}
	else
	{
		BottomMessage::AddMessage(103, text, BottomMessageType_INFO, 180);
	}
}

//上/下 滑行
void Write::UpScrollPage()
{
	static int Speed = WindowSize.y / 35;
	MoveSpeed.y = Speed;
}
void Write::DownScrollPage()
{
	static int Speed = -WindowSize.y / 35;
	MoveSpeed.y = Speed;
}

void Write::ResetMove()
{
	ResetPos.x.SetAnimationStartValue(GetCurPage().CameraPos.x);
	ResetPos.y.SetAnimationStartValue(GetCurPage().CameraPos.y);
	ResetPos.x.SetAnimation(0, 120);
	ResetPos.y.SetAnimation(0, 120);
}

//图片管理
#pragma region MyRegion

bool ImageManager::AddImage(wstring path,RenWin& window)
{
	GetCurPage().Images.emplace_back();
	if (!XImage::NewImage(GetCurPage().Images.back().img, path))
	{
		return false;
	}
	else
	{
		ImageStruct& img = GetCurPage().Images.back();
		SkipImageIndex = GetCurPage().Images.size() - 1;

		Vector2i Pos = { WindowSize.x / 2,WindowSize.y / 2 };

		img.h = img.img.h, img.w = img.img.w;

		if (img.h > WindowSize.y * 0.7)
		{
			img.w = img.img.w * WindowSize.y * 0.7 / img.h;
			img.h = WindowSize.y * 0.7;
		}
		if (img.w > WindowSize.x * 0.7)
		{
			img.h = img.img.h * WindowSize.x * 0.7 / img.w;
			img.w = WindowSize.x * 0.7;
		}

		static int RoundSize = WindowSize.x / 150;

		int ActivePoint = -1;

		//微透明图片
		img.img.color.a = 200;

		//确认框
		static Vector2i EnsureBarSize = {WindowSize.x / 15,WindowSize.y / 22};
		static int Space = WindowSize.x / 100;

		while (!XMsg::IsClose(window))
		{
			XWindow::DelayFps(window,60);
			
			//绘制之前书写
			DrawLayer(window);
			//绘制荧光层
			DrawLightLayer(window);
			//绘制图片层
			DrawImageLayer(window);

			Clamp(Pos.x, WindowSize.x * 0.1, WindowSize.x * 0.9);
			Clamp(Pos.y, WindowSize.y * 0.1, WindowSize.y * 0.9);

			if (img.w <= WindowSize.x * 0.2) img.w = WindowSize.x * 0.2;
			if (img.h <= WindowSize.y * 0.2) img.h = WindowSize.y * 0.2;

			if (img.w > 0 && img.h > 0)
			{
				float ScaleX = img.w / (float)img.img.w, ScaleY = img.h / (float)img.img.h;
				XImage::PutScaleImage(img.img, Pos.x, Pos.y, ScaleX, ScaleY, window, 0.5, 0.5);

				XGraph::SetFillColor(User::MainColor);
				XGraph::SetColor(User::MainColor);
				XGraph::CircleShape::FillCircle_WithoutBorder(Pos.x - img.w / 2, Pos.y - img.h / 2, RoundSize, window);
				XGraph::CircleShape::FillCircle_WithoutBorder(Pos.x + img.w / 2, Pos.y - img.h / 2, RoundSize, window);
				XGraph::CircleShape::FillCircle_WithoutBorder(Pos.x - img.w / 2, Pos.y + img.h / 2, RoundSize, window);
				XGraph::CircleShape::FillCircle_WithoutBorder(Pos.x + img.w / 2, Pos.y + img.h / 2, RoundSize, window);

				static int lw = WindowSize.x / 400;
				XGraph::LineShape::SetLineWidth(lw);
				XGraph::RectangleShape::Rect(Pos.x - img.w / 2, Pos.y - img.h / 2, img.w, img.h, window);

				//绘制勾选框
				XGraph::LineShape::SetLineWidth(lw / 1.5);
				XGraph::SetFillColor(Color(30, 30, 30));
				XGraph::SetColor(Color(100, 255, 100));
				XGraph::RectangleShape::FillRect(Pos.x + img.w / 2 - EnsureBarSize.x - Space, Pos.y + img.h / 2 + Space * 2, EnsureBarSize.x, EnsureBarSize.y, window);
				XGraph::SetColor(Color(255, 100, 100));
				XGraph::RectangleShape::FillRect(Pos.x + img.w / 2 - EnsureBarSize.x * 2 - Space * 2, Pos.y + img.h / 2 + Space * 2, EnsureBarSize.x, EnsureBarSize.y, window);

				static int FontSize = FONTSIZE * 0.8;
				XText::SetFontConfig(Color(100, 255, 100), FontSize);
				XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
				XText::Xyprintf(Pos.x + img.w / 2 - EnsureBarSize.x / 2 - Space, Pos.y + img.h / 2 + Space * 2 + EnsureBarSize.y / 2, "插入", window);
				XText::SetFontConfig(Color(255, 100, 100), FontSize);
				XText::Xyprintf(Pos.x + img.w / 2 - EnsureBarSize.x * 3 / 2 - Space * 2, Pos.y + img.h / 2 + Space * 2 + EnsureBarSize.y / 2, "取消", window);
			}

			if (XMsg::MouseMsg::IsMousePress(VK::MouseLeft))
			{
				Vector2i MousePos = XMsg::MouseMsg::GetMousePosWindow();;
				if (XMsg::MouseMsg::IsMouseIn(Pos.x - img.w / 2 + img.w * 0.1, Pos.y - img.h / 2 + img.h * 0.1, img.w * 0.8, img.h * 0.8))
				{
					Pos = MousePos;

					static int MoveBasicSize = WindowSize.x / 350;

					//右移
					if (Pos.x > WindowSize.x * 2 / 3)
					{
						Pos.x -= MoveBasicSize;
						GetCurPage().CameraPos.x -= MoveBasicSize;
					}
					//左移
					if (Pos.x < WindowSize.x / 3)
					{
						Pos.x += MoveBasicSize;
						GetCurPage().CameraPos.x += MoveBasicSize;
					}
					//上移
					if (Pos.y < WindowSize.y / 3)
					{
						Pos.y += MoveBasicSize;
						GetCurPage().CameraPos.y += MoveBasicSize;
					}
					//下移
					if (Pos.y > WindowSize.y * 2 / 3)
					{
						Pos.y -= MoveBasicSize;
						GetCurPage().CameraPos.y -= MoveBasicSize;
					}
				}
				else if(XMsg::MouseMsg::IsMouseIn(Pos.x - img.w / 2 - img.w * 0.1, Pos.y - img.h / 2 - img.h * 0.1, img.w * 1.2, img.h * 1.2))
				{
					if (ActivePoint < 0)
					{
						if (MousePos.x < WindowSize.x / 2)
						{
							if (MousePos.y < WindowSize.y / 2) ActivePoint = 0;
							else ActivePoint = 3;
						}
						if (MousePos.x > WindowSize.x / 2)
						{
							if (MousePos.y < WindowSize.y / 2) ActivePoint = 1;
							else ActivePoint = 2;
						}
					}
					else
					{
						//左上角
						if (ActivePoint == 0)
						{
							//定点
							Vector2i RightBottomPoint = { Pos.x + img.w / 2,Pos.y + img.h / 2 };

							int NewW = RightBottomPoint.x - MousePos.x;
							int NewH = RightBottomPoint.y - MousePos.y;

							img.w = NewW;
							img.h = NewH;

							Pos.x = RightBottomPoint.x - NewW / 2;
							Pos.y = RightBottomPoint.y - NewH / 2;
						}
						//右上角
						if (ActivePoint == 1)
						{
							//定点：左下角
							Vector2i LeftBottomPoint = { Pos.x - img.w / 2, Pos.y + img.h / 2 };

							int NewW = MousePos.x - LeftBottomPoint.x;
							int NewH = LeftBottomPoint.y - MousePos.y;

							img.w = NewW;
							img.h = NewH;

							Pos.x = LeftBottomPoint.x + NewW / 2;
							Pos.y = LeftBottomPoint.y - NewH / 2;
						}
						//右下角
						if (ActivePoint == 2)
						{
							//定点：左上角
							Vector2i LeftTopPoint = { Pos.x - img.w / 2, Pos.y - img.h / 2 };

							int NewW = MousePos.x - LeftTopPoint.x;
							int NewH = MousePos.y - LeftTopPoint.y;

							img.w = NewW;
							img.h = NewH;

							Pos.x = LeftTopPoint.x + NewW / 2;
							Pos.y = LeftTopPoint.y + NewH / 2;
						}
						//左下角
						if (ActivePoint == 3)
						{
							//定点：右上角
							Vector2i RightTopPoint = { Pos.x + img.w / 2, Pos.y - img.h / 2 };

							int NewW = RightTopPoint.x - MousePos.x;
							int NewH = MousePos.y - RightTopPoint.y;

							img.w = NewW;
							img.h = NewH;

							Pos.x = RightTopPoint.x - NewW / 2;
							Pos.y = RightTopPoint.y + NewH / 2;
						}
					}
				}
			}
			else ActivePoint = -1;

			if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
			{
				//确定
				if (XMsg::MouseMsg::IsMouseIn(Pos.x + img.w / 2 - EnsureBarSize.x - Space, Pos.y + img.h / 2 + Space * 2, EnsureBarSize.x, EnsureBarSize.y))
				{
					float scale = GetCurPage().Scale / 100.0f;
					if (scale <= 0) scale = 1.0f;  // 防止除零

					// 存的尺寸要除以 scale，这样绘制时乘回来刚好一样大
					img.w = img.w / scale;
					img.h = img.h / scale;

					img.pos.x = Pos.x - GetCurPage().CameraPos.x;
					img.pos.y = Pos.y - GetCurPage().CameraPos.y;

					FlushImageLayer = true;

					img.img.color.a = 255;

					break;
				}
				//取消
				if (XMsg::MouseMsg::IsMouseIn(Pos.x + img.w / 2 - EnsureBarSize.x * 2 - Space * 2, Pos.y + img.h / 2 + Space * 2, EnsureBarSize.x, EnsureBarSize.y))
				{
					GetCurPage().Images.pop_back();
					break;
				}
			}
		}

		SkipImageIndex = -1;
		return true;

		XMsg::SetSleepTime(10);
	}
}

bool ImageManager::AddImageDirectly(wstring path)
{
	GetCurPage().Images.emplace_back();
	if (!XImage::NewImage(GetCurPage().Images.back().img, path))
	{
		return false;
	}
	else
	{
		ImageStruct& img = GetCurPage().Images.back();
		SkipImageIndex = GetCurPage().Images.size() - 1;

		Vector2i Pos = { WindowSize.x / 2,WindowSize.y / 2 };

		img.h = img.img.h, img.w = img.img.w;

		if (img.h > WindowSize.y * 0.7)
		{
			img.w = img.img.w * WindowSize.y * 0.7 / img.h;
			img.h = WindowSize.y * 0.7;
		}
		if (img.w > WindowSize.x * 0.7)
		{
			img.h = img.img.h * WindowSize.x * 0.7 / img.w;
			img.w = WindowSize.x * 0.7;
		}

		static int RoundSize = WindowSize.x / 150;

		int ActivePoint = -1;

		//微透明图片
		img.img.color.a = 200;

		//确认框
		static Vector2i EnsureBarSize = { WindowSize.x / 15,WindowSize.y / 22 };
		static int Space = WindowSize.x / 100;

		float scale = GetCurPage().Scale / 100.0f;
		if (scale <= 0) scale = 1.0f;  // 防止除零

		// 存的尺寸要除以 scale，这样绘制时乘回来刚好一样大
		img.w = img.w / scale;
		img.h = img.h / scale;

		img.pos.x = Pos.x - GetCurPage().CameraPos.x;
		img.pos.y = Pos.y - GetCurPage().CameraPos.y;

		FlushImageLayer = true;

		img.img.color.a = 255;

		SkipImageIndex = -1;
		return true;

		XMsg::SetSleepTime(10);
	}
}

// 让相机居中聚焦到世界坐标 target
Vector2i FocusCameraOn(Vector2i target)
{
	auto& pd = GetCurPage();
	float scale = pd.Scale / 100.0f;
	if (scale <= 0) scale = 1.0f;

	Vector2i result;
	result.x = (int)((WindowSize.x / 2.0f) / scale - target.x);
	result.y = (int)((WindowSize.y / 2.0f) / scale - target.y);

	pd.CameraPos = result;
	return result;
}

void EditImage(int i, RenWin& window)
{
	ImageStruct& img = GetCurPage().Images[i];
	Vector2i Pos = { WindowSize.x / 2,WindowSize.y / 2 };

	if (img.h > WindowSize.y * 0.7)
	{
		img.w = img.img.w * WindowSize.y * 0.7 / img.h;
		img.h = WindowSize.y * 0.7;
	}
	if (img.w > WindowSize.x * 0.7)
	{
		img.h = img.img.h * WindowSize.x * 0.7 / img.w;
		img.w = WindowSize.x * 0.7;
	}

	static int RoundSize = WindowSize.x / 150;

	int ActivePoint = -1;

	//微透明图片
	img.img.color.a = 200;

	//确认框
	static Vector2i EnsureBarSize = { WindowSize.x / 15,WindowSize.y / 22 };
	static int Space = WindowSize.x / 100;

	GetCurPage().CameraPos = FocusCameraOn(img.pos);

	FlushImageLayer = true;

	while (!XMsg::IsClose(window))
	{
		XWindow::DelayFps(window,60);

		//绘制之前书写
		DrawLayer(window);
		//绘制荧光层
		DrawLightLayer(window);
		//绘制图片层
		DrawImageLayer(window);

		Clamp(Pos.x, WindowSize.x * 0.1, WindowSize.x * 0.9);
		Clamp(Pos.y, WindowSize.y * 0.1, WindowSize.y * 0.9);

		if (img.w <= WindowSize.x * 0.2) img.w = WindowSize.x * 0.2;
		if (img.h <= WindowSize.y * 0.2) img.h = WindowSize.y * 0.2;

		if (img.w > 0 && img.h > 0)
		{
			float ScaleX = img.w / (float)img.img.w, ScaleY = img.h / (float)img.img.h;
			XImage::PutScaleImage(img.img, Pos.x, Pos.y, ScaleX, ScaleY, window, 0.5, 0.5);

			XGraph::SetFillColor(User::MainColor);
			XGraph::SetColor(User::MainColor);
			XGraph::CircleShape::FillCircle_WithoutBorder(Pos.x - img.w / 2, Pos.y - img.h / 2, RoundSize, window);
			XGraph::CircleShape::FillCircle_WithoutBorder(Pos.x + img.w / 2, Pos.y - img.h / 2, RoundSize, window);
			XGraph::CircleShape::FillCircle_WithoutBorder(Pos.x - img.w / 2, Pos.y + img.h / 2, RoundSize, window);
			XGraph::CircleShape::FillCircle_WithoutBorder(Pos.x + img.w / 2, Pos.y + img.h / 2, RoundSize, window);

			static int lw = WindowSize.x / 400;
			XGraph::LineShape::SetLineWidth(lw);
			XGraph::RectangleShape::Rect(Pos.x - img.w / 2, Pos.y - img.h / 2, img.w, img.h, window);

			//绘制勾选框
			XGraph::LineShape::SetLineWidth(lw / 1.5);
			XGraph::SetFillColor(Color(30, 30, 30));
			XGraph::SetColor(Color(100, 255, 100));
			XGraph::RectangleShape::FillRect(Pos.x + img.w / 2 - EnsureBarSize.x - Space, Pos.y + img.h / 2 + Space * 2, EnsureBarSize.x, EnsureBarSize.y, window);
			XGraph::SetColor(Color(255, 100, 100));
			XGraph::RectangleShape::FillRect(Pos.x + img.w / 2 - EnsureBarSize.x * 2 - Space * 2, Pos.y + img.h / 2 + Space * 2, EnsureBarSize.x, EnsureBarSize.y, window);

			static int FontSize = FONTSIZE * 0.8;
			XText::SetFontConfig(Color(100, 255, 100), FontSize);
			XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
			XText::Xyprintf(Pos.x + img.w / 2 - EnsureBarSize.x / 2 - Space, Pos.y + img.h / 2 + Space * 2 + EnsureBarSize.y / 2, "确认", window);
			XText::SetFontConfig(Color(255, 100, 100), FontSize);
			XText::Xyprintf(Pos.x + img.w / 2 - EnsureBarSize.x * 3 / 2 - Space * 2, Pos.y + img.h / 2 + Space * 2 + EnsureBarSize.y / 2, "删除", window);
		}

		if (XMsg::MouseMsg::IsMousePress(VK::MouseLeft))
		{
			Vector2i MousePos = XMsg::MouseMsg::GetMousePosWindow();;
			if (XMsg::MouseMsg::IsMouseIn(Pos.x - img.w / 2 + img.w * 0.1, Pos.y - img.h / 2 + img.h * 0.1, img.w * 0.8, img.h * 0.8))
			{
				Pos = MousePos;

				static int MoveBasicSize = WindowSize.x / 350;

				//右移
				if (Pos.x > WindowSize.x * 2 / 3)
				{
					Pos.x -= MoveBasicSize;
					GetCurPage().CameraPos.x -= MoveBasicSize;
				}
				//左移
				if (Pos.x < WindowSize.x / 3)
				{
					Pos.x += MoveBasicSize;
					GetCurPage().CameraPos.x += MoveBasicSize;
				}
				//上移
				if (Pos.y < WindowSize.y / 3)
				{
					Pos.y += MoveBasicSize;
					GetCurPage().CameraPos.y += MoveBasicSize;
				}
				//下移
				if (Pos.y > WindowSize.y * 2 / 3)
				{
					Pos.y -= MoveBasicSize;
					GetCurPage().CameraPos.y -= MoveBasicSize;
				}
			}
			else if (XMsg::MouseMsg::IsMouseIn(Pos.x - img.w / 2 - img.w * 0.1, Pos.y - img.h / 2 - img.h * 0.1, img.w * 1.2, img.h * 1.2))
			{
				if (ActivePoint < 0)
				{
					if (MousePos.x < WindowSize.x / 2)
					{
						if (MousePos.y < WindowSize.y / 2) ActivePoint = 0;
						else ActivePoint = 3;
					}
					if (MousePos.x > WindowSize.x / 2)
					{
						if (MousePos.y < WindowSize.y / 2) ActivePoint = 1;
						else ActivePoint = 2;
					}
				}
				else
				{
					//左上角
					if (ActivePoint == 0)
					{
						//定点
						Vector2i RightBottomPoint = { Pos.x + img.w / 2,Pos.y + img.h / 2 };

						int NewW = RightBottomPoint.x - MousePos.x;
						int NewH = RightBottomPoint.y - MousePos.y;

						img.w = NewW;
						img.h = NewH;

						Pos.x = RightBottomPoint.x - NewW / 2;
						Pos.y = RightBottomPoint.y - NewH / 2;
					}
					//右上角
					if (ActivePoint == 1)
					{
						//定点：左下角
						Vector2i LeftBottomPoint = { Pos.x - img.w / 2, Pos.y + img.h / 2 };

						int NewW = MousePos.x - LeftBottomPoint.x;
						int NewH = LeftBottomPoint.y - MousePos.y;

						img.w = NewW;
						img.h = NewH;

						Pos.x = LeftBottomPoint.x + NewW / 2;
						Pos.y = LeftBottomPoint.y - NewH / 2;
					}
					//右下角
					if (ActivePoint == 2)
					{
						//定点：左上角
						Vector2i LeftTopPoint = { Pos.x - img.w / 2, Pos.y - img.h / 2 };

						int NewW = MousePos.x - LeftTopPoint.x;
						int NewH = MousePos.y - LeftTopPoint.y;

						img.w = NewW;
						img.h = NewH;

						Pos.x = LeftTopPoint.x + NewW / 2;
						Pos.y = LeftTopPoint.y + NewH / 2;
					}
					//左下角
					if (ActivePoint == 3)
					{
						//定点：右上角
						Vector2i RightTopPoint = { Pos.x + img.w / 2, Pos.y - img.h / 2 };

						int NewW = RightTopPoint.x - MousePos.x;
						int NewH = MousePos.y - RightTopPoint.y;

						img.w = NewW;
						img.h = NewH;

						Pos.x = RightTopPoint.x - NewW / 2;
						Pos.y = RightTopPoint.y + NewH / 2;
					}
				}
			}
		}
		else ActivePoint = -1;

		if (XMsg::MouseMsg::IsMouseDown(VK::MouseLeft))
		{
			//确定
			if (XMsg::MouseMsg::IsMouseIn(Pos.x + img.w / 2 - EnsureBarSize.x - Space, Pos.y + img.h / 2 + Space * 2, EnsureBarSize.x, EnsureBarSize.y))
			{
				float scale = GetCurPage().Scale / 100.0f;
				if (scale <= 0) scale = 1.0f;  // 防止除零

				// 存的尺寸要除以 scale，这样绘制时乘回来刚好一样大
				img.w = img.w / scale;
				img.h = img.h / scale;

				img.pos.x = Pos.x - GetCurPage().CameraPos.x;
				img.pos.y = Pos.y - GetCurPage().CameraPos.y;

				FlushImageLayer = true;

				img.img.color.a = 255;

				break;
			}
			//删除
			if (XMsg::MouseMsg::IsMouseIn(Pos.x + img.w / 2 - EnsureBarSize.x * 2 - Space * 2, Pos.y + img.h / 2 + Space * 2, EnsureBarSize.x, EnsureBarSize.y))
			{
				GetCurPage().Images.erase(GetCurPage().Images.begin() + i);
				break;
			}
		}
	}

	FlushWriteLayer = FlushImageLayer = true;

	XMsg::ClearMsg();
	XMsg::SetSleepTime(10);
}

#pragma endregion

//文件管理
#pragma region MyRegion

// 导出分辨率倍率：2x 保证清晰，可调 3x（文件更大）
static const float EXPORT_RES_SCALE = 2.0f;

// ===== 思维导图导出绘制（无按钮，底线延展替代） =====
static void MindMapDrawToExport(RenderTarget& rt, float scale, float camX, float camY,
	float offsetX, float offsetY)
{
	if (!MindMapActived || MindMapNodes.empty()) return;

	// 世界坐标 -> 导出画布坐标
	// 调用方保证 camX = camY = 0，offsetX = offsetY = pad
	auto EX = [&](int x) { return (float)(x * scale + camX + offsetX); };
	auto EY = [&](int y) { return (float)(y * scale + camY + offsetY); };

	int gap = MindMapLineGap();
	int lw = max(1, (int)(max(2, WindowSize.y / 260) * scale));

	// ① 父子连接线：从「延展底线末端」出发
	XGraph::LineShape::SetLineWidth((float)lw);
	for (auto& n : MindMapNodes)
	{
		float extendedLineW = n.lineEase.value + (float)MindMapBtnZoneW();
		float px = EX(n.x + (int)extendedLineW);
		float py = EY(n.y + n.lineH + gap);

		for (int c : n.children)
		{
			int ci = MindMapIndexOf(c);
			if (ci < 0) continue;

			auto& child = MindMapNodes[ci];
			float cx = EX(child.x);
			float cy = EY(child.y + child.lineH + gap);

			Color lc = n.color;
			lc.a = 255;
			XGraph::SetColor(lc);

			int detachC = max(1, (int)(MindMapLinkDetach() * scale));

			bool straightLine = (n.children.size() == 1) ||
				(abs((int)(cy - py)) <= max(2, (int)(4 * scale)));

			if (straightLine)
			{
				XGraph::LineShape::Line(px, py, cx, (float)(cy - detachC), rt);
				continue;
			}

			float hSpan = (float)abs((int)(cx - px));
			float ctrl = hSpan * 0.55f;
			if (ctrl < 1.0f) ctrl = 1.0f;

			float y3 = cy - detachC;
			float c1x = px + ctrl, c1y = py + (y3 - py) * 0.2f;
			float c2x = cx - ctrl, c2y = y3 - (y3 - py) * 0.2f;

			auto CubicAt = [](float p0, float p1, float p2, float p3, float t)
				{
					float mt = 1.0f - t;
					return mt * mt * mt * p0
						+ 3.0f * mt * mt * t * p1
						+ 3.0f * mt * t * t * p2
						+ t * t * t * p3;
				};

			const int SEG = 32;
			float lastX = px, lastY = py;
			for (int s = 1; s <= SEG; ++s)
			{
				float t = (float)s / SEG;
				float nx = CubicAt(px, c1x, c2x, cx, t);
				float ny = CubicAt(py, c1y, c2y, y3, t);
				XGraph::LineShape::Line(lastX, lastY, nx, ny, rt);
				lastX = nx; lastY = ny;
			}
		}
	}

	// ② 节点底线：延展到按钮区右缘，不画任何按钮
	for (auto& n : MindMapNodes)
	{
		float extendedLineW = n.lineEase.value + (float)MindMapBtnZoneW();
		float lineY = EY(n.y + n.lineH + gap);
		float x1 = EX(n.x);
		float x2 = EX(n.x + (int)extendedLineW);

		XGraph::SetColor(n.color);
		XGraph::LineShape::SetLineWidth((float)lw);
		XGraph::LineShape::Line(x1, lineY, x2, lineY, rt);
	}
}

// 判断某页是否存在需要导出的实际内容（笔画或图片）
static bool PageHasContent(const PageDataS& pd)
{
	if (!pd.Images.empty()) return true;
	for (size_t i = 0; i < pd.Data.size(); ++i)
	{
		if (!pd.Data[i].empty()) return true;
	}
	return false;
}

// 把指定板书页完整渲染到纹理
static void RenderPageToTexture(const PageDataS& pd, RenderTexture& rt, bool drawMindMap = false)
{
	rt.clear(Color(30, 30, 30));

	float scale = pd.Scale / 100.0f;
	if (scale <= 0) scale = 1.0f;

	// 导出专用缩放
	float exportScale = scale * EXPORT_RES_SCALE;
	const float pad = 40.f * ScreenScale * EXPORT_RES_SCALE;

	// 计算内容包围盒（世界坐标）
	float minX = FLT_MAX, minY = FLT_MAX;
	float maxX = -FLT_MAX, maxY = -FLT_MAX;

	auto expandByStroke = [&](float x, float y, float x2, float y2, float w)
		{
			float halfW = max(w, 1.f) * 1.5f;
			minX = min(minX, min(x, x2) - halfW);
			maxX = max(maxX, max(x, x2) + halfW);
			minY = min(minY, min(y, y2) - halfW);
			maxY = max(maxY, max(y, y2) + halfW);
		};

	for (size_t i = 0; i < pd.Data.size(); ++i)
	{
		for (size_t k = 0; k < pd.Data[i].size(); ++k)
		{
			const WriteData& data = pd.Data[i][k];
			expandByStroke(data.x, data.y, data.x2, data.y2, data.w);
			if (!(data.StartX < 0 && data.StartY < 0))
				expandByStroke(data.StartX, data.StartY, data.StartX, data.StartY, data.w);
		}
	}

	for (size_t i = 0; i < pd.Images.size(); i++)
	{
		const ImageStruct& img = pd.Images[i];
		if (img.w <= 0 || img.h <= 0) continue;
		float diag = (float)std::hypot((double)img.w, (double)img.h) * 0.5f;
		minX = min(minX, img.pos.x - diag);
		maxX = max(maxX, img.pos.x + diag);
		minY = min(minY, img.pos.y - diag);
		maxY = max(maxY, img.pos.y + diag);
	}

	if (drawMindMap && MindMapActived && !MindMapNodes.empty())
	{
		int mmGap = MindMapLineGap();
		for (auto& n : MindMapNodes)
		{
			float l = (float)n.x;
			float r = (float)(n.x + (int)n.lineEase.value + MindMapBtnZoneW());
			float t = (float)(n.y - n.lineH / 2);
			float b = (float)(n.y + n.lineH + mmGap);
			minX = min(minX, l);
			maxX = max(maxX, r);
			minY = min(minY, t);
			maxY = max(maxY, b);
		}
	}

	if (minX > maxX || minY > maxY)
	{
		minX = minY = 0.f;
		maxX = WindowSize.x / scale;
		maxY = WindowSize.y / scale;
	}

	// 世界坐标 -> 导出画布坐标（不含 CameraPos，避免内容偏移错位）
	auto W2E_X = [&](float x) { return x * exportScale + pad; };
	auto W2E_Y = [&](float y) { return y * exportScale + pad; };

	// 图片层
	for (size_t i = 0; i < pd.Images.size(); i++)
	{
		const ImageStruct& img = pd.Images[i];
		float sx = W2E_X(img.pos.x);
		float sy = W2E_Y(img.pos.y);
		float sw = img.w * exportScale;
		float sh = img.h * exportScale;
		if (sw <= 0 || sh <= 0 || img.img.w <= 0 || img.img.h <= 0) continue;
		float ScaleX = sw / (float)img.img.w;
		float ScaleY = sh / (float)img.img.h;
		XImage::PutRoteScaleImage(
			const_cast<IMAGE&>(img.img), sx, sy, (float)img.rote,
			ScaleX, ScaleY, rt, 0.5, 0.5);
	}

	// 笔画层
	for (size_t i = 0; i < pd.Data.size(); ++i)
	{
		for (size_t k = 0; k < pd.Data[i].size(); ++k)
		{
			const WriteData& data = pd.Data[i][k];
			float sx1 = W2E_X(data.x);
			float sy1 = W2E_Y(data.y);
			float sx2 = W2E_X(data.x2);
			float sy2 = W2E_Y(data.y2);

			XGraph::LineShape::SetLineWidth(data.w * exportScale);
			if (data.StartX < 0 && data.StartY < 0)
			{
				XGraph::SetColor(data.color);
				XGraph::LineShape::Line(sx1, sy1, sx2, sy2, rt);
			}
			else
			{
				float ssx1 = W2E_X(data.StartX);
				float ssy1 = W2E_Y(data.StartY);
				DrawFilledPolygon({ {(int)ssx1,(int)ssy1},{(int)sx1,(int)sy1},{(int)sx2,(int)sy2} }, data.color, rt);
			}
		}
	}

	// 叠加思维导图
	if (drawMindMap)
		MindMapDrawToExport(rt, exportScale, 0.f, 0.f, pad, pad);

	rt.display();
}

// 把当前有内容的板书页导出为图片文件
static int SaveAsImage(const wstring& dir, const wstring& ext)
{
	// 导出纹理尺寸 = 窗口尺寸 × 分辨率倍率
	unsigned int rtW = static_cast<unsigned int>(WindowSize.x * EXPORT_RES_SCALE);
	unsigned int rtH = static_cast<unsigned int>(WindowSize.y * EXPORT_RES_SCALE);
	RenderTexture rt(sf::Vector2u(rtW, rtH));

	vector<const PageDataS*> pages;
	vector<bool> pageIsCurrent;

	for (size_t i = 0; i < PageData.size(); i++)
	{
		if (!PageHasContent(PageData[i])) continue;
		pages.push_back(&PageData[i]);
		pageIsCurrent.push_back(!WriteCamera::EnableWriteCamera && (int)i == Write::Page);
	}
	for (size_t i = 0; i < CameraPageData.size(); i++)
	{
		if (!PageHasContent(CameraPageData[i])) continue;
		pages.push_back(&CameraPageData[i]);
		pageIsCurrent.push_back(WriteCamera::EnableWriteCamera && (int)i == Write::Page);
	}

	int saved = 0;
	for (size_t i = 0; i < pages.size(); i++)
	{
		RenderPageToTexture(*pages[i], rt, pageIsCurrent[i]);
		wstring outPath = dir + L"\\" + to_wstring(i + 1) + L"." + ext;
		sf::Image img = rt.getTexture().copyToImage();
		if (img.saveToFile(outPath))
			saved++;
	}

	return saved;
}

void WriteFile::Save(wstring path, std::wstring ExtName)
{
	wstring extLower = ExtName;
	for (auto& ch : extLower) ch = (wchar_t)towlower(ch);

	XFile::CreateDirectory(path);

	size_t slashPos = path.find_last_of(L"\\/");
	wstring fileName = (slashPos == wstring::npos) ? path : path.substr(slashPos + 1);

	if (extLower != L"mwf")
	{
		int saved = SaveAsImage(path, extLower);

		if (saved > 0)
		{
			if (1 == Message::ShowMessage("已导出图片", "保存成功", ICOTYPE_SUCCESS, { "确定","打开文件夹" }, 1, L"MiuBarrd"))
			{
				ShellExecuteW(nullptr, L"open", L"explorer.exe",
					path.c_str(), nullptr, SW_SHOWNORMAL);
			}
			XWindow::SetBackGroundColor(Color(30, 30, 30));
		}
		else
		{
			Message::ShowMessage("导出图片失败", "保存失败", ICOTYPE_ERROR, { "确定" }, 3, L"MiuBarrd");
			XWindow::SetBackGroundColor(Color(30, 30, 30));
		}
		return;
	}

	ofstream save(path + L"\\" + fileName + L".mwf");
	if (!save.is_open())
	{
		Message::ShowMessage("导出图片失败", "保存失败", ICOTYPE_ERROR, { "确定" }, 3, L"MiuBarrd");
		XWindow::SetBackGroundColor(Color(30, 30, 30));
		return;
	}

	for (size_t i = 0; i < PageData.size(); i++)
	{
		if (PageData[i].Data.empty()) continue;

		save << "Page======" << endl;
		auto& page = PageData[i];

		for (size_t k = 0; k < page.Data.size(); k++)
		{
			auto& stroke = page.Data[k];
			for (size_t c = 0; c < stroke.size(); c++)
			{
				auto& d = stroke[c];
				save << "Stroke====== "
					<< d.x << " " << d.y << " " << d.x2 << " " << d.y2 << " "
					<< d.StartX << " " << d.StartY << " "
					<< (int)d.color.r << " " << (int)d.color.g << " "
					<< (int)d.color.b << " " << (int)d.color.a << " "
					<< d.Light << " " << d.TempLayer << " " << d.w << endl;
			}
		}
	}

	save.close();

	if (1 == Message::ShowMessage("已导出图片", "保存成功", ICOTYPE_SUCCESS, { "确定","打开文件夹" }, 1, L"MiuBarrd"))
	{
		ShellExecuteW(nullptr, L"open", L"explorer.exe",
			path.c_str(), nullptr, SW_SHOWNORMAL);
	}
	XWindow::SetBackGroundColor(Color(30, 30, 30));
}

void WriteFile::Load(wstring path)
{
	ifstream file(path);
	if (!file.is_open())
	{
		RightMessage::ShowMessage(L"打开板书文件失败", L"加载板书", RightMessageType_ERROR, false);
		BottomMessage::AddMessage(110, L"加载板书失败", BottomMessageType_ERROR, 240);
		return;
	}

	if (PageData.empty()) return;

	if (!PageData[0].Data.empty())
	{
		int r = Message::ShowMessage("是否丢弃之前的板书?", "导入文件", ICOTYPE_QUESTION, { "丢弃", "保存" }, 1, L"MiuBarrd");
		if (r == 1)
		{
			wstring P = XFile::GetDir::Desktop().wstring()
				+ L"\\板书"
				+ to_wstring(XTime::GetTimeNow_Day())
				+ to_wstring(XTime::GetTimeNow_Hour())
				+ to_wstring(XTime::GetTimeNow_Min())
				+ L".mwf";
			Save(P, L"桌面");
		}
	}

	XWindow::SetBackGroundColor(Color(30, 30, 30));

	PageData.clear();

	string line;
	int curPageIndex = -1;

	while (getline(file, line))
	{
		if (line.find("Page======") == 0)
		{
			PageData.push_back(PageDataS());
			Write::TotalPage += 1;
			curPageIndex = (int)PageData.size() - 1;
		}
		else if (line.find("Stroke======") == 0)
		{
			if (curPageIndex < 0 || curPageIndex >= (int)PageData.size()) continue;

			WriteData d;
			istringstream iss(line.substr(12));

			int ir = 255, ig = 255, ib = 255, ia = 255;
			if (!(iss >> d.x >> d.y >> d.x2 >> d.y2
				>> d.StartX >> d.StartY
				>> ir >> ig >> ib >> ia
				>> d.Light >> d.TempLayer >> d.w))
			{
				continue;
			}

			d.color = Color(ir, ig, ib, ia);
			PageData[curPageIndex].Data.push_back(vector<WriteData>());
			PageData[curPageIndex].Data.back().push_back(d);
		}
	}

	PageData.erase(
		remove_if(PageData.begin(), PageData.end(),
			[](const PageDataS& p)
			{
				if (p.Data.empty()) return true;
				for (size_t i = 0; i < p.Data.size(); ++i)
					if (!p.Data[i].empty()) return false;
				return true;
			}),
		PageData.end());

	if (PageData.empty())
		PageData.push_back(PageDataS());

	Write::TotalPage = (int)PageData.size();
	if (Write::Page < 0) Write::Page = 0;
	if (Write::Page > Write::TotalPage - 1) Write::Page = Write::TotalPage - 1;

	file.close();
	BottomMessage::AddMessage(110, L"已加载板书", BottomMessageType_SUCCESS, 240);

	FlushWriteLayer = FlushImageLayer = true;
}

#pragma endregion

//展台
#pragma region MyRegion

void WriteCamera::Start()
{
	static int InitW = WindowSize.y * 0.8,InitH = WindowSize.x * 0.8;

	int DI = 0;
	ifstream DeviceIndex("DeviceIndex.ini");
	if (DeviceIndex.is_open())
	{
		DeviceIndex >> DI;
		DeviceIndex.close();
	}
	else
	{
		DeviceIndex.close();

		ofstream DeviceIndexInit("DeviceIndex.ini");
		DeviceIndexInit << "0";

		DeviceIndexInit.close();
	}

	if (!CameraManager::Init(DI, InitW, InitH))
	{
		WriteCamera::EnableWriteCamera = false;

		BottomMessage::AddMessage(301, L"无法打开视频展台", BottomMessageType_ERROR);
		RightMessage::ShowMessage(L"无法打开视频展台", L"视频展台", RightMessageType_ERROR, true);
		RightMessage::ShowMessage(L"无法打开视频展台", L"视频展台", RightMessageType_ERROR, true);
	}
	else
	{
		WriteCamera::EnableWriteCamera = true;
	}
}

void WriteCamera::Draw(RenWin& window)
{
	auto& pd = GetCurPage();

	if (!WriteCamera::EnableWriteCamera || Write::Page != 0) return;

	float Scale = pd.Scale / 100.0;

	static int WW = WindowSize.x / 2, WH = WindowSize.y / 2;

	float dx = (pd.CameraPos.x + WW) * Scale;
	float dy = (pd.CameraPos.y + WH) * Scale;

	CameraManager::DrawCameraImage(
		dx,dy, Scale,
		window);

	if (CameraManager::NeedAutoPhoto()) PhotoImage();

	if (EnableAutoPhoto)
	{
		XGraph::SetFillColor(Color(30, 30, 30, 150));
		static int w = WindowSize.x / 4, h = WindowSize.y / 15,r = WindowSize.x / 200;
		static int x = WindowSize.x / 2 - w / 2, y = WindowSize.y / 2 - h / 2;
		XGraph::RectangleShape::FillRoundRect_WithoutBorder(x, y, w, h, r, window);
		XText::SetFontConfig(Color::White, FONTSIZE * 1.5);
		XText::SetFontAdjust(ADJUST_CENTER, ADJUST_CENTER);
		XText::Xyprintf(WindowSize.x / 2, WindowSize.y / 2, L"自动拍照，请保持静止", window);
	}
}

void WriteCamera::Stop()
{
	CameraManager::Stop();
	WriteCamera::EnableWriteCamera = false;
}

bool WriteCamera::Manage()
{
	if (WriteCamera::EnableWriteCamera) Stop();
	else Start();

	if (WriteCamera::EnableWriteCamera) BottomMessage::AddMessage(201, L"已加载展台", BottomMessageType_INFO);
	else BottomMessage::AddMessage(201, L"已关闭展台", BottomMessageType_INFO);

	static int LastPage = Write::Page;
	static int LastTotalPage = Write::TotalPage;
	if (WriteCamera::EnableWriteCamera) 
	{
		LastPage = Write::Page;
		LastTotalPage = Write::TotalPage;
		Write::Page = 0;
		Write::TotalPage = CameraPageData.size();
	}
	else
	{
		Write::Page = LastPage;
		Write::TotalPage = LastTotalPage;
	}

	FrontAlpha = 255;
	
	FlushImageLayer = FlushWriteLayer = true;
	
	Tool::UpdatePage();

	return WriteCamera::EnableWriteCamera;
}

bool WriteCamera::IsRun()
{
	return WriteCamera::EnableWriteCamera;
}

void WriteCamera::PhotoImage()
{
	if(Write::Page == 0)
	{
		// 先取图再切页：tex 只在 Write::Page == 0 时由 WriteCamera::Draw 刷新，
		// 若先切页会取到上一帧甚至空纹理
		CameraPageData.emplace_back();

		auto& newPage = CameraPageData.back();

		newPage.Images.emplace_back();

		CameraManager::Get(newPage.Images[0].img);
		newPage.Images[0].w = CameraManager::GetW();
		newPage.Images[0].h = CameraManager::GetH();
		newPage.Images[0].pos.x = WindowSize.x / 2;
		newPage.Images[0].pos.y = WindowSize.y / 2;

		newPage.Images[0].rote = CameraManager::Rotation.value;

		ImgScan::Correct(newPage.Images[0].img.texture);


		Write::TotalPage = CameraPageData.size();

		FlushImageLayer = FlushWriteLayer = true;

		Tool::UpdatePage();

		FrontAlpha = 255;

		BottomMessage::AddMessage(201, L"已保存到相册", BottomMessageType_SUCCESS);

		Tool::ShowPhotoWindow();
	}
	else
	{
		Write::Page = 0;

		Tool::UpdateCameraControl();

		CameraManager::SleepUpdate(0);

		FlushImageLayer = FlushWriteLayer = true;
	}

	Tool::UpdatePage();

	
}

void WriteCamera::Rote()
{
	if (!WriteCamera::EnableWriteCamera) return;

	if (Write::Page == 0)
	{
		CameraManager::Rote();
	}
	else
	{
		CameraPageData[Write::Page].Images[0].rote -= 90;
		if (CameraPageData[Write::Page].Images[0].rote <= -360) CameraPageData[Write::Page].Images[0].rote += 360;

		FlushImageLayer = true;
	}
}

void WriteCamera::ManageAutoPhoto()
{
	EnableAutoPhoto = !EnableAutoPhoto;

	if (EnableAutoPhoto)
	{
		BottomMessage::AddMessage(401, L"开始自动拍照", BottomMessageType_INFO);
	}
	else
	{
		BottomMessage::AddMessage(401, L"停止自动拍照", BottomMessageType_INFO);
	}
}

IMAGE& WriteCamera::GetPhoto(int page)
{
	if (page < 1) page = 1;

	return CameraPageData[page].Images[0].img;
}

int WriteCamera::GetRote(int page)
{
	if (page < 1) page = 1;

	return CameraPageData[page].Images[0].rote;
}

#pragma endregion


