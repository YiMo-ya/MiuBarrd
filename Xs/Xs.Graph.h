#pragma once
#include "Xs.Main.h"

namespace xs
{

	//线样式
enum LineType
{
	LINE_SOLID,//实线
	LINE_CENTER,//虚线
	LINE_DOTTED//点线
};

//线帽样式
enum LineCapType
{
	LINECAP_FLAT,//平头
	LINECAP_ROUND,//圆头
	LINECAP_SQUART//方头
};

//图形
namespace XGraph
{
	/// <summary>
	/// 设置全局线条颜色
	/// </summary>
	/// <param name="color">要设置的颜色</param>
	void SetColor(Color color);
	/// <summary>
	/// 设置全局填充颜色
	/// </summary>
	/// <param name="color">要设置的颜色</param>
	void SetFillColor(Color color);

	namespace LineShape
	{
	
		/// <summary>
		/// 绘制直线
		/// </summary>
		/// <param name="x1">起始坐标x</param>
		/// <param name="y1">起始坐标y</param>
		/// <param name="x2">终点坐标x</param>
		/// <param name="y2">终点坐标y</param>
		/// <param name="Dest">目标窗口</param>
		void Line(float x1, float y1, float x2, float y2,RenderTarget& Dest);

		/// <summary>
		/// 绘制直线
		/// </summary>
		/// <param name="x1">起始坐标x</param>
		/// <param name="y1">起始坐标y</param>
		/// <param name="x2">终点坐标x</param>
		/// <param name="y2">终点坐标y</param>
		/// <param name="thickness">线粗细</param>
		/// <param name="type">线形状</param>
		/// <param name="Dest">目标窗口</param>
		void LineEx(float x, float y, float x2, float y2, int thickness, LineType type, RenderTarget& Dest);

		/// <summary>
		/// 设置全局线条粗细，此函数也会影响其他绘制函数的线条粗细
		/// </summary>
		/// <param name="thickness">粗细</param>
		void SetLineWidth(int thickness = 1);

		/// <summary>
		/// 设置线条类型
		/// </summary>
		/// <param name="type">类型，可为：实线：LINE_SOLID；虚线：LINE_CENTER；点线：LINE_DOTTED</param>
		void SetLineType(LineType type = LINE_SOLID);

		/// <summary>
		/// 设置线头类型
		/// </summary>
		/// <param name="type">类型，可为：平头：LINECAP_FLAT；圆头：LINECAP_ROUND；方头：LINECAP_SQUART</param>
		void SetLineCap(LineCapType type = LINECAP_ROUND);
	};
	namespace CircleShape
	{
	
		/// <summary>
		/// 绘制圆形
		/// </summary>
		/// <param name="x">中心x</param>
		/// <param name="y">中心y</param>
		/// <param name="radius">半径</param>
		/// <param name="Dest">目标窗口</param>
		void Circle(float x, float y, float radius, RenderTarget& Dest);
		/// <summary>
		/// 绘制填充的圆形，带边框
		/// </summary>
		/// <param name="x">中心x</param>
		/// <param name="y">中心y</param>
		/// <param name="radius">半径</param>
		/// <param name="Dest">目标窗口</param>
		void FillCircle(float x, float y, float radius, RenderTarget& Dest);
		/// <summary>
		/// 绘制椭圆，带边框
		/// </summary>
		/// <param name="x">中心x</param>
		/// <param name="y">中心y</param>
		/// <param name="radiusX">横向半径</param>
		/// <param name="radiusY">纵向半径</param>
		/// <param name="Dest">目标窗口</param>
		void Ellipse(float x, float y, float radiusX, float radiusY, RenderTarget& Dest);
		/// <summary>
		/// 绘制填充的椭圆，带边框
		/// </summary>
		/// <param name="x">中心x</param>
		/// <param name="y">中心y</param>
		/// <param name="radiusX">横向半径x</param>
		/// <param name="radiusY">横向半径y</param>
		/// <param name="Dest">目标窗口</param>
		void FillEllipse(float x, float y, float radiusX, float radiusY, RenderTarget& Dest);
		/// <summary>
		/// 绘制填充无边框圆
		/// </summary>
		/// <param name="x">中心x</param>
		/// <param name="y">中心y</param>
		/// <param name="radius">半径</param>
		/// <param name="Dest">目标窗口</param>
		void FillCircle_WithoutBorder(float x, float y, float radius, RenderTarget& Dest);
		/// <summary>
		/// 绘制填充无边框椭圆
		/// </summary>
		/// <param name="x">中心x</param>
		/// <param name="y">中心y</param>
		/// <param name="radiusX">横向半径x</param>
		/// <param name="radiusY">横向半径y</param>
		/// <param name="Dest">目标窗口</param>
		void FillEllipse_WithoutBorder(float x, float y, float radiusX, float radiusY, RenderTarget& Dest);
		/// <summary>
		/// 绘制圆弧
		/// </summary>
		/// <param name="x">中心x</param>
		/// <param name="y">中心y</param>
		/// <param name="radius">半径</param>
		/// <param name="startAngle">起始角度</param>
		/// <param name="endAngle">绘制角度</param>
		/// <param name="Dest">目标窗口</param>
		void Arc(float x, float y, float radius, float startAngle, float endAngle, RenderTarget& Dest);
		/// <summary>
		/// 绘制无边框填充圆弧
		/// </summary>
		/// <param name="x">中心x</param>
		/// <param name="y">中心y</param>
		/// <param name="radius">半径</param>
		/// <param name="startAngle">起始角度</param>
		/// <param name="endAngle">绘制角度</param>
		/// <param name="Dest">目标窗口</param>
		void FillArc_WithoutBorder(float x, float y, float radius, float startAngle, float endAngle, RenderTarget& Dest);
		/// <summary>
		/// 绘制填充圆弧，带边框
		/// </summary>
		/// <param name="x">中心x</param>
		/// <param name="y">中心y</param>
		/// <param name="radius">半径</param>
		/// <param name="startAngle">起始角度</param>
		/// <param name="endAngle">绘制角度</param>
		/// <param name="Dest">目标窗口</param>
		void FillArc(float x, float y, float radius, float startAngle, float endAngle, RenderTarget& Dest);
	};
	namespace RectangleShape
	{
	
		/// <summary>
		/// 绘制矩形边框
		/// </summary>
		/// <param name="x">左上角x</param>
		/// <param name="y">左上角y</param>
		/// <param name="w">宽</param>
		/// <param name="h">高</param>
		/// <param name="Dest">目标窗口</param>
		void Rect(float x, float y, float w, float h, RenderTarget& Dest);
		/// <summary>
		/// 绘制填充矩形，带边框
		/// </summary>
		/// <param name="x">左上角x</param>
		/// <param name="y">左上角y</param>
		/// <param name="w">宽</param>
		/// <param name="h">高</param>
		/// <param name="Dest">目标窗口</param>
		void FillRect(float x, float y, float w, float h, RenderTarget& Dest);
		/// <summary>
		/// 绘制填充无边框矩形
		/// </summary>
		/// <param name="x">左上角x</param>
		/// <param name="y">左上角y</param>
		/// <param name="w">宽</param>
		/// <param name="h">高</param>
		/// <param name="Dest">目标窗口</param>
		void FillRect_WithoutBorder(float x, float y, float w, float h, RenderTarget& Dest);
		/// <summary>
		/// 绘制圆角矩形边框
		/// </summary>
		/// <param name="x">左上角x</param>
		/// <param name="y">左上角y</param>
		/// <param name="w">宽</param>
		/// <param name="h">高</param>
		/// <param name="radius">半径</param>
		/// <param name="Dest">目标窗口</param>
		void RoundRect(float x, float y, float w, float h, float radius, RenderTarget& Dest);
		/// <summary>
		/// 绘制填充圆角矩形，带边框
		/// </summary>
		/// <param name="x">左上角x</param>
		/// <param name="y">左上角y</param>
		/// <param name="w">宽</param>
		/// <param name="h">高</param>
		/// <param name="radius">半径</param>
		/// <param name="Dest">目标窗口</param>
		void FillRoundRect(float x, float y, float w, float h, float radius, RenderTarget& Dest);
		/// <summary>
		/// 绘制填充无边框圆角矩形
		/// </summary>
		/// <param name="x">左上角x</param>
		/// <param name="y">左上角y</param>
		/// <param name="w">宽</param>
		/// <param name="h">高</param>
		/// <param name="radius">半径</param>
		/// <param name="Dest">目标窗口</param>
		void FillRoundRect_WithoutBorder(float x, float y, float w, float h, float radius, RenderTarget& Dest);
	};
};

//图像
struct ximage
{
	sf::Texture texture;
	sf::Color color = sf::Color::White;
	int w = 0;
	int h = 0;
};

using IMAGE = ximage;

//图像
namespace XImage
{
	/// <summary>
	/// 创建图像
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="filename">文件路径</param>
	/// <param name="x">索取图像的x坐标</param>
	/// <param name="y">索取图像的y坐标</param>
	/// <param name="w">索取图像的宽</param>
	/// <param name="h">索取图像的高</param>
	/// <returns></returns>
	bool NewImage(IMAGE& img,const String& filename, int x = 0, int y = 0, int w = 0, int h = 0);

	/// <summary>
	/// 从窗口上获取图像
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	/// <param name="w">宽</param>
	/// <param name="h">高</param>
	bool NewImage(RenWin& window,IMAGE& img, int x = 0, int y = 0, int w = 0, int h = 0);

	/// <summary>
	/// 从另一图像上获取图像，目标图像不保留原有的颜色滤镜，但是src会以颜色滤镜的形式覆盖dest
	/// </summary>
	/// <param name="dest">目标图像</param>
	/// <param name="src">原图像</param>
	/// <param name="x">获取的x坐标</param>
	/// <param name="y">获取的y坐标</param>
	/// <param name="w">获取的宽</param>
	/// <param name="h">获取的高</param>
	/// <returns></returns>
	bool NewImage(IMAGE& dest, IMAGE& src, int x = -1, int y = -1, int w = -1, int h = -1);

	/// <summary>
	/// 裁剪图片
	/// </summary>
	/// <param name="img">需要裁剪的图片</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	/// <param name="w">裁剪宽度</param>
	/// <param name="h">裁剪高度</param>
	/// <returns></returns>
	bool CutImage(IMAGE& img, int x, int y, int w, int h);

	/// <summary>
	/// 启用平滑图片，默认是
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="smooth">是否平滑</param>
	void SetImageSmooth(IMAGE& img, bool smooth);

	/// <summary>
	/// 是否平铺，默认否
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="repeated">是否平铺</param>
	void SetImageRepeated(IMAGE& img, bool repeated);

	/// <summary>
	/// 应用图像颜色滤镜
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="color">颜色</param>
	void SetImageColor(IMAGE& img, Color color);

	/// <summary>
	/// 更新图像，其目标不保留原有的颜色滤镜
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="dest">目标图像</param>
	void UpdateImage(IMAGE& img, IMAGE& dest);

	/// <summary>
	/// 半透明化图像，alpha为0时完全透明，alpha为255时完全不透明
	/// </summary>
	/// <param name="img">目标图像</param>
	/// <param name="alpha">不透明值</param>
	void AlphaImage(IMAGE& img, int alpha);

	/// <summary>
	/// 绘制图像到窗口
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	/// <param name="Dest">目标窗口</param>
	void PutImage(IMAGE& img, float x, float y, RenderTarget& Dest,float CenterX = 0.0,float CenterY = 0.0);

	/// <summary>
	/// 绘制图像到另一图像
	/// </summary>
	/// <param name="dest">目标图像</param>
	/// <param name="src">原图像</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	void PutImage(IMAGE& dest, IMAGE& src, float x, float y, float CenterX = 0.0, float CenterY = 0.0);

	/// <summary>
	/// 绘制旋转图像到窗口
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	/// <param name="angle">角度</param>
	/// <param name="Dest">目标窗口</param>
	void PutRoteImage(IMAGE& img, float x, float y, float angle, RenderTarget& Dest, float CenterX = 0.0, float CenterY = 0.0);

	/// <summary>
	/// 绘制旋转图像到另一图像，目标图像不保留原有的颜色滤镜，但是img会以颜色滤镜的形式覆盖dest
	/// </summary>
	/// <param name="dest">目标图像</param>
	/// <param name="img">原图像</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	/// <param name="angle">角度</param>
	void PutRoteImage(IMAGE& dest,IMAGE& img, float x, float y, float angle, float CenterX = 0.0, float CenterY = 0.0);

	/// <summary>
	/// 绘制缩放图像到窗口
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	/// <param name="scaleX">缩放宽倍数</param>
	/// <param name="scaleY">缩放高倍数</param>
	/// <param name="Dest">目标窗口</param>
	void PutScaleImage(IMAGE& img, float x, float y, float scaleX, float scaleY, RenderTarget& Dest, float CenterX = 0.0, float CenterY = 0.0);

	/// <summary>
	/// 绘制缩放图像到另一图像，目标图像不保留原有的颜色滤镜，但是img会以颜色滤镜的形式覆盖dest
	/// </summary>
	/// <param name="dest">目标图像</param>
	/// <param name="img">原图像</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	/// <param name="scaleX">缩放宽倍数</param>
	/// <param name="scaleY">缩放高</param>
	void PutScaleImage(IMAGE& dest, IMAGE& img, float x, float y, float scaleX, float scaleY, float CenterX = 0.0, float CenterY = 0.0);

	/// <summary>
	/// 绘制旋转缩放图像到窗口
	/// </summary>
	/// <param name="img">IMAGE</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	/// <param name="angle">角度</param>
	/// <param name="scaleX">缩放宽倍数</param>
	/// <param name="scaleY">缩放高</param>
	/// <param name="Dest">目标窗口</param>
	void PutRoteScaleImage(IMAGE& img, float x, float y, float angle, float scaleX, float scaleY, RenderTarget& Dest, float CenterX = 0.0, float CenterY = 0.0);

	/// <summary>
	/// 绘制旋转缩放图像到另一图像，目标图像不保留原有的颜色滤镜，但是img会以颜色滤镜的形式覆盖dest
	/// </summary>
	/// <param name="dest">目标图像</param>
	/// <param name="img">原图像</param>
	/// <param name="x">x坐标</param>
	/// <param name="y">y坐标</param>
	/// <param name="angle">角度</param>
	/// <param name="scaleX">缩放宽倍数</param>
	/// <param name="scaleY">缩放高</param>
	void PutRoteScaleImage(IMAGE& dest, IMAGE& img, float x, float y, float angle, float scaleX, float scaleY, float CenterX = 0.0, float CenterY = 0.0);

	/// <summary>
	/// 获取图像的宽
	/// </summary>
	/// <param name="img">目标图像</param>
	/// <returns></returns>
	int GetWidth(IMAGE& img);

	/// <summary>
	/// 获取图像的高
	/// </summary>
	/// <param name="img">目标图像</param>
	/// <returns></returns>
	int GetHeight(IMAGE& img);

	/// <summary>
	/// 设置图像大小，当其中值小于0时忽略
	/// </summary>
	void SetImageSize(IMAGE& img,int x = -1, int y = -1);

	/// <summary>
	/// 清理图像为纯色背景
	/// </summary>
	/// <param name="image">目标图像</param>
	/// <param name="backcolor">目标颜色</param>
	void ClearImage(IMAGE& image, Color backcolor);
};


// 高级图像处理
namespace XImageEx
{

	//==================== 模糊 ====================
	namespace ImageBlur
	{
	
		/// <summary>
		/// 高斯模糊
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="sigma">模糊强度，推荐 2.0~4.0</param>
		void Gaussian(IMAGE& img, double sigma);

		/// <summary>
		/// 盒式模糊
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="radius">模糊半径，推荐 2~6</param>
		void Box(IMAGE& img, std::size_t radius);

		/// <summary>
		/// 运动模糊
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="angleDeg">方向角度（0° 右，90° 下）</param>
		/// <param name="intensity">强度，推荐 0.15~0.35</param>
		/// <param name="samples">采样数，推荐 8~16</param>
		void Motion(IMAGE& img, float angleDeg, float intensity = 0.25f,
			std::size_t samples = 12);

		/// <summary>
		/// 径向模糊
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="intensity">强度，推荐 0.25~0.5</param>
		/// <param name="samples">采样数，推荐 10~16</param>
		/// <param name="centerX">中心X（归一化 0~1）</param>
		/// <param name="centerY">中心Y（归一化 0~1）</param>
		void Radial(IMAGE& img, float intensity = 0.35f,
			std::size_t samples = 12,
			float centerX = 0.5f, float centerY = 0.5f);

		/// <summary>
		/// 旋转模糊
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="angleDeg">旋转角度，推荐 15~90</param>
		/// <param name="samples">采样数，推荐 10~20</param>
		/// <param name="centerX">中心X（归一化 0~1）</param>
		/// <param name="centerY">中心Y（归一化 0~1）</param>
		void Spin(IMAGE& img, float angleDeg = 45.0f,
			std::size_t samples = 12,
			float centerX = 0.5f, float centerY = 0.5f);

		/// <summary>
		/// 朦胧图像
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="intensity">强度，推荐 0.2~0.5</param>
		/// <param name="maxAlphaMask">遮罩最大透明度，推荐 180~220</param>
		/// <param name="sigma">模糊强度，推荐 5.0~10.0</param>
		/// <param name="layerStep">层次衰减，推荐 0.05~0.15</param>
		void Hazy(IMAGE& img, float intensity,
			int maxAlphaMask = 200,
			double sigma = 7.0f,
			float layerStep = 0.1f);
	};

	//==================== 扭曲 ====================
	namespace ImageDistort
	{
	
		/// <summary>
		/// 鱼眼
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="intensity">强度，推荐 0.25~0.5</param>
		/// <param name="centerX">中心X（归一化 0~1）</param>
		/// <param name="centerY">中心Y（归一化 0~1）</param>
		void Fisheye(IMAGE& img, float intensity = 0.35f,
			float centerX = 0.5f, float centerY = 0.5f);

		/// <summary>
		/// 热浪
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="amplitude">振幅，推荐 0.1~0.2</param>
		/// <param name="frequency">频率，推荐 20~30</param>
		/// <param name="phase">相位（随时间递增）</param>
		/// <param name="noiseStrength">噪声强度，推荐 0.3~0.5</param>
		void Heat(IMAGE& img, float amplitude = 0.15f,
			float frequency = 25.0f, float phase = 0.0f,
			float noiseStrength = 0.4f);

		/// <summary>
		/// 黑洞
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="intensity">强度，推荐 0.3~0.6</param>
		/// <param name="centerX">中心X（归一化 0~1）</param>
		/// <param name="centerY">中心Y（归一化 0~1）</param>
		/// <param name="power">衰减指数，推荐 1.5~2.5</param>
		void BlackHole(IMAGE& img, float intensity = 0.45f,
			float centerX = 0.5f, float centerY = 0.5f,
			float power = 2.0f);

		/// <summary>
		/// 水面波动
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="centerX">中心X（归一化 0~1）</param>
		/// <param name="centerY">中心Y（归一化 0~1）</param>
		/// <param name="radius">环半径X</param>
		/// <param name="radius">环半径Y</param>
		/// <param name="width">环宽度</param>
		/// <param name="amplitude">扭曲强度</param>
		void WaterRing(IMAGE& img,
			float centerX,
			float centerY,
			float radiusX,
			float radiusY,
			float width,
			float amplitude);
	};

	//==================== 羽化 ====================
	namespace ImageFeather
	{
	
		/// <summary>
		/// 圆形羽化
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="intensity">强度，推荐 0.2~0.4</param>
		void Circle(IMAGE& img, float intensity = 0.3f);

		/// <summary>
		/// 方形羽化
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="intensity">强度，推荐 0.2~0.4</param>
		void Rect(IMAGE& img, float intensity = 0.3f);
	};

	//==================== 特效 ====================
	namespace ImageEffect
	{
	
		/// <summary>
		/// 色差
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="intensity">强度，推荐 0.1~0.25</param>
		/// <param name="centerX">中心X（归一化 0~1）</param>
		/// <param name="centerY">中心Y（归一化 0~1）</param>
		void Chromatic(IMAGE& img, float intensity = 0.2f,
			float centerX = 0.5f, float centerY = 0.5f);

		/// <summary>
		/// 马赛克
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="blockSize">方块大小，推荐 4~16</param>
		void Mosaic(IMAGE& img, float blockSize);

		/// <summary>
		/// 故障特效
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="intensity">强度，推荐 0.15~0.4</param>
		///  <param name="maxLineWidth">最大线宽度</param>
		/// <param name="lineDensity">线密度</param>
		void Glitch(IMAGE& img, float intensity = 0.25f, float maxLineWidth = 0.1,float lineDensity = 0.4);
	};

	//==================== 颜色变换 ====================
	namespace ImageColorTransforms
	{
	
		/// <summary>
		/// 黑白化
		/// </summary>
		/// <param name="img">目标图像</param>
		void Gray(IMAGE& img);

		/// <summary>
		/// 反色
		/// </summary>
		/// <param name="img">目标图像</param>
		void Invert(IMAGE& img);

		/// <summary>
		/// 对比度
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="contrast">对比度，推荐 0.8~1.4</param>
		void Contrast(IMAGE& img, float contrast);

		/// <summary>
		/// 亮度
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="brightness">亮度偏移，推荐 -0.2~0.3</param>
		void Brightness(IMAGE& img, float brightness);

		/// <summary>
		/// 饱和度
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="saturation">饱和度，推荐 0.0~1.5</param>
		void Saturation(IMAGE& img, float saturation);

		/// <summary>
		/// Gamma 校正
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="gamma">Gamma值，标准 2.2，推荐 0.8~2.2</param>
		void Gamma(IMAGE& img, float gamma);
	};

	//==================== 颜色替换 ====================
	namespace ImageReplaceColor
	{
	

		/// <summary>
		/// RGB 颜色替换（性能高，适合精确换色）
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="targetColor">目标颜色</param>
		/// <param name="newColor">新颜色</param>
		/// <param name="tolerance">容差，推荐 8~20（0~255）</param>
		void ReplaceColorRGB(IMAGE& img,
			const sf::Color& targetColor,
			const sf::Color& newColor,
			std::uint8_t tolerance = 12);
	};

	//==================== 缩放 ====================
	namespace ImageChange
	{
	
		/// <summary>
		/// 缩放图像（特效前常用 0.5 以提升性能）
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="scaleX">横向缩放，0.5=一半，2.0=两倍</param>
		/// <param name="scaleY">纵向缩放，0.5=一半，2.0=两倍</param>
		void Scale(IMAGE& img, float scaleX, float scaleY);

		/// <summary>
		/// 旋转图片
		/// </summary>
		/// <param name="img">目标图像</param>
		/// <param name="angleDeg">要旋转的角度</param>
		void Rotate(IMAGE& img, float angleDeg);
	};
};

//文字对齐方式
enum FontAdjustType
{
	ADJUST_LEFT,//左对齐
	ADJUST_RIGHT,//右对齐
	ADJUST_CENTER,//中心对齐
	ADJUST_TOP,//上对齐
	ADJUST_BOTTOM//下对齐
};

//文字绘制
namespace XText
{

	/// <summary>
	/// 在指定的位置绘制文本
	/// </summary>
	/// <param name="x">坐标：x</param>
	/// <param name="y">坐标：y</param>
	/// <param name="str">需要绘制的文本</param>
	/// <param name="Dest">目标窗口</param>
	void Xyprintf(float x, float y, const std::string& str, RenderTarget& Dest);
	/// <summary>
	/// 在指定的位置绘制文本
	/// </summary>
	/// <param name="x">坐标：x</param>
	/// <param name="y">坐标：y</param>
	/// <param name="str">需要绘制的文本</param>
	/// <param name="Dest">目标窗口</param>
	void Xyprintf(float x, float y, const std::wstring& str, RenderTarget& Dest);

	/// <summary>
	/// 设置绘制文本的颜色
	/// </summary>
	/// <param name="color">要设置的颜色</param>
	void SetFontColor(Color color);
	/// <summary>
	/// 设置绘制文本的大小
	/// </summary>
	/// <param name="size">要设置的大小</param>
	void SetFontSize(int size);
	/// <summary>
	/// 设置绘制文本的配置
	/// </summary>
	/// <param name="color">要设置的颜色</param>
	/// <param name="size">要设置的大小</param>
	void SetFontConfig(Color color, int size);

	/// <summary>
	/// 设置字体
	/// </summary>
	/// <param name="font">要设置的字体</param>
	void SetFont(sf::Font font);
	/// <summary>
	/// 从Font类型设置字体
	/// </summary>
	/// <param name="font">要设置的字体</param>
	/// <param name="color">要设置的颜色</param>
	/// <param name="size">要设置的大小</param>
	void SetFont(sf::Font font, Color color, int size);
	/// <summary>
	/// 从文件中设置字体
	/// </summary>
	/// <param name="filename">文件名</param>
	void SetFont(const String& filename);
	/// <summary>
	/// 从文件中设置字体
	/// </summary>
	/// <param name="filename">文件名</param>
	/// <param name="color">要设置的颜色</param>
	/// <param name="size">要设置的大小</param>
	void SetFont(const String& filename, Color color, int size);

	/// <summary>
	/// 查找并设置本地安装的字体
	/// </summary>
	/// <param name="fontname">字体名称</param>
	void FindFont(const String& fontname);

	/// <summary>
	/// 设置文本对齐方式
	/// </summary>
	/// <param name="horizontal">水平对齐方式，有以下值：ADJUST_LEFT 左对齐；ADJUST_RIGHT 右对齐；ADJUST_CENTER 中心对齐</param>
	/// <param name="vertical">水平对齐方式，有以下值：ADJUST_TOP 上对齐；ADJUST_BOTTOM 下对齐；ADJUST_CENTER 中心对齐</param>
	void SetFontAdjust(FontAdjustType horizontal, FontAdjustType vertical);
	/// <summary>
	/// 测量指定文本总宽度
	/// </summary>
	/// <param name="str">文本</param>
	/// <param name="fontsize">字体大小</param>
	/// <returns>总宽度</returns>
	int GetFontWidth(const String& str, unsigned int fontsize = 0);
	/// <summary>
	/// 测量指定文本总高度
	/// </summary>
	/// <param name="str">文本</param>
	/// <param name="fontsize">字体大小</param>
	/// <returns>总高度</returns>
	int GetFontHeight(const String& str, unsigned int fontsize = 0);
};

}