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

#pragma endregion

//启用展台
bool WriteCamera::EnableWriteCamera = false;
bool WriteCamera::EnableAutoPhoto = false;

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
//返回值：展台开启且当前页为展台专用页(1000)时返回 CameraPageData，否则返回普通板书页数据
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
bool NeedErase(const int& sx1, const int& sy1, const int& sx2, const int& sy2, bool IsLeaf = false,const int& ssx1 = -1,const int& ssy1 = -1)
{
	if (!IsErasing || Tool::IsInBar) return false;

	Vector2i erasePos = XMsg::MouseMsg::GetMousePosWindow();

	int ex = erasePos.x - Write::EraseSize / ScreenScale * EraseStf;
	int ey = erasePos.y - Write::EraseSize * 1.4 / ScreenScale * EraseStf;

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
		if (sx1 > ex && sy1 > ey && sx1 < ex + rw && sy1 < ey + rh)
			return true;
		if (sx2 > ex && sy2 > ey && sx2 < ex + rw && sy2 < ey + rh)
			return true;
	}
	else if(ssx1 > -1 && ssy1 > -1)
	{
		if (sx1 > ex && sy1 > ey && sx1 < ex + rw && sy1 < ey + rh)
			return true;
		if (sx2 > ex && sy2 > ey && sx2 < ex + rw && sy2 < ey + rh)
			return true;

		if (LineHitRect(ssx1, ssy1, sx1, sy1, ex, ey, rw, rh)) return true;
		if (LineHitRect(ssx1, ssy1, sx2, sy2, ex, ey, rw, rh)) return true;
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

bool FlushWriteLayer = false;
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
							if (NeedErase(sx1, sy1, sx2, sy2))
							{
								swap(pd.Data[i][k], pd.Data[i].back());
								pd.Data[i].pop_back();
								--k;
								continue;
							}
						}
						else
						{
							if (NeedErase(sx1, sy1, sx2, sy2, true, ssx1, ssy1))
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
				if (data.w < Tool::PenSize)data.w += 0.4 * scale;
			}
			else
			{
				if (data.w < Tool::PenSize / 3)data.w += 0.1 * scale;
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

bool FlushImageLayer = false;
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
		sf::ContextSettings rtSettings;
		rtSettings.antiAliasingLevel = User::AA;

		sf::Vector2u rtSize(
			static_cast<unsigned int>(WindowSize.x),
			static_cast<unsigned int>(WindowSize.y)
		);

		WriteTempLayer = RenderTexture(rtSize, rtSettings);

		WriteLayer = RenderTexture(rtSize, rtSettings);
		LightLayer = RenderTexture(rtSize, rtSettings);
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

//判断某页是否存在需要导出的实际内容（笔画或图片）。
//导出时用它过滤掉"仅被占位创建、实际为空"的页面，避免多导出不存在的页。
static bool PageHasContent(const PageDataS& pd)
{
	if (!pd.Images.empty()) return true;

	for (size_t i = 0; i < pd.Data.size(); ++i)
	{
		if (!pd.Data[i].empty()) return true;
	}

	return false;
}

//把指定板书页完整渲染到纹理：背景填充 Color(30,30,30)，依次绘制图片层与笔画层。
//与旧实现不同，这里不再以窗口为画布，而是取"该页全部内容的世界坐标包围盒"作为渲染范围，
//因此无论页面被移动到何处（即使已移出视口被裁掉），都会连同四周留白一起完整画入。
//pd 为页面数据；rt 为目标纹理，调用方需保证其尺寸与窗口一致。
static void RenderPageToTexture(const PageDataS& pd, RenderTexture& rt)
{
	// 背景统一为深灰，保证导出 PNG 不透明且与软件画布观感一致
	rt.clear(Color(30, 30, 30));

	float scale = pd.Scale / 100.0f;
	if (scale <= 0) scale = 1.0f;

	// 世界坐标 -> 导出画布坐标的偏移：内容包围盒左上角对齐到留白处，
	// 减去 CameraPos 让导出结果与用户当前看到的相对位置保持一致。
	const float pad = 40.f * ScreenScale;   // 四周预留空隙（按屏幕缩放自适应）

	// 先计算所有内容在世界坐标下的包围盒（含线宽外扩），保证被裁掉的部分也被纳入
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

		// 图片以中心定位，旋转时用对角线做保守外扩，避免旋转后被切角
		float diag = (float)std::hypot((double)img.w, (double)img.h) * 0.5f;
		minX = min(minX, img.pos.x - diag);
		maxX = max(maxX, img.pos.x + diag);
		minY = min(minY, img.pos.y - diag);
		maxY = max(maxY, img.pos.y + diag);
	}

	// 空页兜底：没有内容时退化为对当前视图渲染一块背景
	if (minX > maxX || minY > maxY)
	{
		minX = minY = 0.f;
		maxX = WindowSize.x / scale;
		maxY = WindowSize.y / scale;
	}

	// 世界坐标由 CameraPos 平移、scale 缩放映射到屏幕；这里直接沿用同一变换，
	// 再把包围盒左上角对齐到 (pad, pad)。
	float camScaleX = pd.CameraPos.x * scale;
	float camScaleY = pd.CameraPos.y * scale;

	float originX = minX * scale + camScaleX;   // 内容左上角在旧坐标系中的位置
	float originY = minY * scale + camScaleY;

	// offset 使"内容左上角"最终落在 (pad, pad) 处
	float offsetX = pad - originX;
	float offsetY = pad - originY;

	// 先绘制图片层，图片置底、笔画覆盖其上（与画布层叠顺序一致）
	for (size_t i = 0; i < pd.Images.size(); i++)
	{
		const ImageStruct& img = pd.Images[i];

		float sx = img.pos.x * scale + camScaleX + offsetX;
		float sy = img.pos.y * scale + camScaleY + offsetY;
		float sw = img.w * scale;
		float sh = img.h * scale;

		// 图片尺寸保护：宽度或高度非正时 PutRoteScaleImage 会产生非法缩放
		if (sw <= 0 || sh <= 0 || img.img.w <= 0 || img.img.h <= 0) continue;

		float ScaleX = sw / (float)img.img.w;
		float ScaleY = sh / (float)img.img.h;

		// Copy 版本的 Image 结构含 texture，旋转中心取 0.5,0.5 与画布绘制保持一致
		XImage::PutRoteScaleImage(
			const_cast<IMAGE&>(img.img), sx, sy, (float)img.rote,
			ScaleX, ScaleY, rt, 0.5, 0.5);
	}

	// 再绘制该页所有笔画
	for (size_t i = 0; i < pd.Data.size(); ++i)
	{
		for (size_t k = 0; k < pd.Data[i].size(); ++k)
		{
			const WriteData& data = pd.Data[i][k];

			float sx1 = data.x * scale + camScaleX + offsetX;
			float sy1 = data.y * scale + camScaleY + offsetY;
			float sx2 = data.x2 * scale + camScaleX + offsetX;
			float sy2 = data.y2 * scale + camScaleY + offsetY;

			XGraph::LineShape::SetLineWidth(data.w * scale);
			if (data.StartX < 0 && data.StartY < 0)
			{
				XGraph::SetColor(data.color);
				XGraph::LineShape::Line(sx1, sy1, sx2, sy2, rt);
			}
			else
			{
				float ssx1 = data.StartX * scale + camScaleX + offsetX;
				float ssy1 = data.StartY * scale + camScaleY + offsetY;
				DrawFilledPolygon({ {(int)ssx1,(int)ssy1},{(int)sx1,(int)sy1},{(int)sx2,(int)sy2} }, data.color, rt);
			}
		}
	}

	rt.display();
}

//把当前有内容的板书页导出为图片文件（每页一张）。
//dir 为目标文件夹（必须已存在），ext 为扩展名（形如 "png"，不含点）。
//每页按数字命名：1.png、2.png …… 序号与"实际导出的页"连续对应。
//只有存在实际内容（笔画或图片）的页才会被导出，空占位页会被跳过。
//普通板书页与展台专用页都会被导出，且包含图片与四周留白。
//返回成功导出的页数。
static int SaveAsImage(const wstring& dir, const wstring& ext)
{
	// 导出纹理尺寸跟随窗口，保证导出清晰度与原板书一致
	sf::Vector2u rtSize(
		static_cast<unsigned int>(WindowSize.x),
		static_cast<unsigned int>(WindowSize.y));
	RenderTexture rt(rtSize);

	// 汇总需要导出的页面：普通板书页 + 展台专用页。
	// 关键：跳过没有任何内容的页，避免把"根本不存在的第 2 页"也导出。
	vector< const PageDataS* > pages;
	for (size_t i = 0; i < PageData.size(); i++)
		if (PageHasContent(PageData[i])) pages.push_back(&PageData[i]);
	for (size_t i = 0; i < CameraPageData.size(); i++)
		if (PageHasContent(CameraPageData[i])) pages.push_back(&CameraPageData[i]);

	int saved = 0;

	for (size_t i = 0; i < pages.size(); i++)
	{
		// 直接渲染该页（含图片层与背景色），渲染范围取内容包围盒，自动保留被裁掉的部分与四周留白
		RenderPageToTexture(*pages[i], rt);

		// 按数字命名，序号跟随实际导出顺序连续递增
		wstring outPath = dir + L"\\" + to_wstring(i + 1) + L"." + ext;

		// copyToImage 会把 RenderTexture 的 Y 翻转摆正，避免导出图片上下颠倒
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

	// 先按 path 创建同名文件夹，所有导出内容统一放入该文件夹内，
	// 避免同一次保存产生的多个文件散落在桌面或其它目录中。
	// 若目录已存在则 CreateDirectory 返回 false，此处不做失败处理，直接复用已有目录。
	XFile::CreateDirectory(path);

	// 取原 path 最后一段作为板书文件名：调用方传入的 path 是完整路径，
	// 例如 "C:\Users\xx\Desktop\板书20247123"，末段即 "板书20247123"。
	size_t slashPos = path.find_last_of(L"\\/");
	wstring fileName = (slashPos == wstring::npos) ? path : path.substr(slashPos + 1);

	if (extLower != L"mwf")
	{
		// 图片统一按数字命名写入 path 文件夹：1.png、2.png 
		int saved = SaveAsImage(path, extLower);

		if (saved > 0)
		{
			if (1 == Message::ShowMessage("已导出图片", "保存成功", ICOTYPE_SUCCESS, { "确定","打开文件夹" }, 1, L"MiuBarrd"))
			{
				ShellExecuteW(nullptr, L"open", L"explorer.exe",
					path.c_str(),
					nullptr, SW_SHOWNORMAL);
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

	// 板书文件写入 path 文件夹内，文件名沿用原 path 末段 + .mwf
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
			path.c_str(),
			nullptr, SW_SHOWNORMAL);
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

	if(!PageData[0].Data.empty())
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

	// 用下标而非指针记录当前页：push_back 触发 vector 扩容时，
	// 之前取到的 &PageData.back() 会立刻变为悬垂指针，后续写入即访问冲突。
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
			// 尚未遇到 "Page======" 时出现 Stroke 行属于脏数据，直接跳过
			if (curPageIndex < 0 || curPageIndex >= (int)PageData.size()) continue;

			// 先完整解析成临时对象，解析成功后才写入，避免"先 push 再回滚"，
			// 既不会误删上一条有效数据，也不会在失败时改动页面结构。
			WriteData d;
			istringstream iss(line.substr(12));

			int ir = 255, ig = 255, ib = 255, ia = 255;
			if (!(iss >> d.x >> d.y >> d.x2 >> d.y2
				>> d.StartX >> d.StartY
				>> ir >> ig >> ib >> ia
				>> d.Light >> d.TempLayer >> d.w))
			{
				continue; // 解析失败直接丢弃，不影响已有数据
			}

			d.color = Color(ir, ig, ib, ia);

			// 每解析一条有效的 Stroke 行就是一个独立的笔画块：
			// 若沿用已存在的块直接追加，会把同一行的多点拼到一起，
			// 这里按行新建块，保证与保存时的层级（Page > Block > 点）一致。
			PageData[curPageIndex].Data.push_back(vector<WriteData>());
			PageData[curPageIndex].Data.back().push_back(d);
		}
		// 图片相关逻辑已彻底移除
	}

	// 清理空页：同时剔除完全没有笔画的页，避免留下空壳页参与后续渲染
	PageData.erase(
		remove_if(PageData.begin(), PageData.end(),
			[](const PageDataS& p)
			{
				if (p.Data.empty()) return true;
				// 块存在但内部无点，同样视为空页
				for (size_t i = 0; i < p.Data.size(); ++i)
					if (!p.Data[i].empty()) return false;
				return true;
			}),
		PageData.end());

	// 兜底：至少保留一页，保证后续 Page 索引始终合法
	if (PageData.empty())
		PageData.push_back(PageDataS());

	// 重新同步页码状态：清空/重建后 TotalPage、Page 必须落在合法范围内，
	// 否则后续 GetCurPage() 会以越界下标访问 PageData 造成访问冲突。
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
