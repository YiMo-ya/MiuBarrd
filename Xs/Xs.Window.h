#pragma once
#include "Xs.Main.h"
#include "Xs.Graph.h"

namespace xs
{
	//窗口圆角类型
	enum WindowCorner
	{
		CORNER_ROUND,//圆角
		CORNER_SMALLROUND,//小圆角
		CORNER_NOROUND//方角
	};

	//窗口背景
	enum WindowBackType
	{
		BACKTYPE_AUTO,//自动
		BACKTYPE_BLUR,//模糊
		BACKTYPE_HEAVYMICA,//重度云母
		BACKTYPE_MICA,//云母
		BACKTYPE_NULL//无
	};

	//窗口
	namespace XWindow
	{
	
		/// <summary>
		/// 高级OpenGL设置
		/// </summary>
		namespace Advanced
		{
		
			/// <summary>
			/// 设置窗口的抗锯齿大小
			/// </summary>
			/// <param name="level">要设置的抗锯齿大小</param>
			/// <param name="注意">请在调用窗口创建函数前设置</param>
			void SetWindowAntiAliasingLevel(unsigned int level);
			/// <summary>
			/// 设置深度缓冲位数
			/// </summary>
			/// <param name="Bits">要设置的位数</param>
			/// <param name="注意">请在调用窗口创建函数前设置</param>
			void SetDepthBits(unsigned int Bits);
			/// <summary>
			/// 设置模板缓冲位数
			/// </summary>
			/// <param name="Bits">要设置的位数</param>
			/// <param name="注意">请在调用窗口创建函数前设置</param>
			void SetStencilBits(unsigned int Bits);
			/// <summary>
			/// 设置窗口要使用的OpenGL版本
			/// </summary>
			/// <param name="version">要设置的版本</param>
			/// <param name="注意">请在调用窗口创建函数前设置</param>
			void SetOpenGLVersion(unsigned int version);
			/// <summary>
			/// 设置窗口上下文属性
			/// </summary>
			/// <param name="flags">要设置的属性</param>
			/// <param name="注意">请在调用窗口创建函数前设置</param>
			void SetAttributeFlags(ContextSettings::Attribute flags);
			/// <summary>
			/// 设置sRGB缓冲
			/// </summary>
			/// <param name="enable">是否启用</param>
			/// <param name="注意">请在调用窗口创建函数前设置</param>
			void SetSRgbCapable(bool enable);
		};

		/// <summary>
		/// 启用程序的高DPI感知
		/// </summary>
		void SetDPIAware();
		/// <summary>
		/// 创建窗口
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <param name="x">坐标：x，小于0时默认</param>
		/// <param name="y">坐标：y，小于0时默认</param>
		/// <param name="w">窗口宽，小于0时默认</param>
		/// <param name="h">窗口高，小于0时默认</param>
		/// <param name="title">窗口标题，空值时默认</param>
		/// <param name="style">窗口样式</param>
		/// <param name="state">窗口类型</param>
		/// <param name="setting">OpenGL设置</param>
		void CreateGraphWindow(RenWin& Window, int x = -1, int y = -1, int w = -1, int h = -1,std::string title = "window", uint32_t style = 7U, sf::State state = State::Windowed);
		/// <summary>
		/// 创建窗口
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <param name="x">坐标：x，小于0时默认</param>
		/// <param name="y">坐标：y，小于0时默认</param>
		/// <param name="w">窗口宽，小于0时默认</param>
		/// <param name="h">窗口高，小于0时默认</param>
		/// <param name="title">窗口标题，空值时默认</param>
		/// <param name="style">窗口样式</param>
		/// <param name="state">窗口类型</param>
		/// <param name="setting">OpenGL设置</param>
		void CreateGraphWindow(RenWin& Window, int x = -1, int y = -1, int w = -1, int h = -1, std::wstring title = L"window", uint32_t style = 7U, sf::State state = State::Windowed);
		/// <summary>
		/// 帧刷新窗口
		/// </summary>
		/// <param name="fps">帧率上限</param>
		/// <param name="Window">目标窗口</param>
		void DelayFps(RenWin& Window, int fps = 30);

		/// <summary>
		/// 设置背景颜色
		/// </summary>
		/// <param name="color">背景颜色</param>
		void SetBackGroundColor(const Color& color = Color::Black);
		/// <summary>
		/// 获取当前背景颜色
		/// </summary>
		/// <returns>返回当前背景颜色</returns>
		Color GetBackGroundColor();
		/// <summary>
		/// 判断当前窗口是否为焦点窗口
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <returns></returns>
		bool IsFocus(RenWin& Window);

		/// <summary>
		/// 设置窗口标题
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <param name="title">标题</param>
		void SetTitle(RenWin& Window, const String& title);

		/// <summary>
		/// 获取标题
		/// </summary>
		/// <returns>标题</returns>
		String GetTitle();

		/// <summary>
		/// 从文件中设置窗口的图标
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <param name="path">文件路径</param>
		void SetIcon(RenWin& Window, const String& path);
		/// <summary>
		/// 从图片中加载图标
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <param name="image">图标图片</param>
		void SetIcon(RenWin& Window, IMAGE& image);

		/// <summary>
		/// 设置鼠标在窗口上是否可见
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <param name="visible">是否可见</param>
		void SetMouseCursorVisible(RenWin& Window, bool visible);

		/// <summary>
		/// 移动窗口
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <param name="x">坐标：x</param>
		/// <param name="y">坐标：y</param>
		void MoveWindow(RenWin& Window, int x, int y);

		/// <summary>
		/// 设置窗口大小
		/// </summary>
		/// <param name="Window">目标大小</param>
		/// <param name="width">设置的宽</param>
		/// <param name="height">设置的高</param>
		void SetWindowSize(RenWin& Window, int width, int height);

		/// <summary>
		/// 获取窗口大小
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <returns></returns>
		Vector2i GetWindowSize(RenWin& Window);

		/// <summary>
		/// 获取窗口坐标
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <returns></returns>
		Vector2i GetWindowPos(RenWin& Window);

		/// <summary>
		/// 设置窗口大小的最小值
		/// </summary>
		/// <param name="Window">目标窗口</param>
		/// <param name="width">最小的宽</param>
		/// <param name="height">最小的高</param>
		void SetWindowMinSize(RenWin& Window, int Width, int Height);

		/// <summary>
		/// 设置鼠标是否可见
		/// </summary>
		/// <param name="window">目标窗口</param>
		/// <param name="visible">是否可见</param>
		void SetMouseVisible(RenWin& Window,bool Visible);

		/// <summary>
		/// 设置垂直同步
		/// </summary>
		/// <param name="window">目标窗口</param>
		/// <param name="enable">是否启用</param>
		void SetVerticalSync(RenWin& Window,bool enable);

		/// <summary>
		/// 最大化窗口
		/// </summary>
		/// <param name="window">目标窗口</param>
		void MaxWindow(RenWin& Window);
		/// <summary>
		/// 最小化窗口
		/// </summary>
		/// <param name="window">目标窗口</param>
		void MinWindow(RenWin& Window);
		/// <summary>
		/// 还原窗口
		/// </summary>
		/// <param name="window">目标窗口</param>
		void RestoreWindow(RenWin& Window);
		/// <summary>
		/// 设置窗口是否可见
		/// </summary>
		/// <param name="window">目标窗口</param>
		void SetWindowVisible(RenWin& Window,bool IsVisible);

		/// <summary>
		/// 为指定窗口添加窗口样式
		/// </summary>
		/// <param name="hwnd">目标窗口句柄</param>
		/// <param name="style">要添加的样式</param>
		/// <returns></returns>
		bool AddWindowStyle(HWND Hwnd, LONG Style);
		/// <summary>
		/// 为指定窗口添加窗口样式
		/// </summary>
		/// <param name="window">目标窗口</param>
		/// <param name="style">要添加的样式</param>
		/// <returns></returns>
		bool AddWindowStyle(RenWin& Window, LONG Style);

		/// <summary>
		/// 为指定窗口添加拓展窗口样式
		/// </summary>
		/// <param name="hwnd">目标窗口句柄</param>
		/// <param name="style">要添加的样式</param>
		/// <returns></returns>
		bool AddWindowExStyle(HWND Hwnd, LONG Style);
		/// <summary>
		/// 为指定窗口添加拓展窗口样式
		/// </summary>
		/// <param name="window">目标窗口</param>
		/// <param name="style">要添加的样式</param>
		/// <returns></returns>
		bool AddWindowExStyle(RenWin& Window, LONG Style);

		/// <summary>
		/// 为指定窗口移除窗口样式
		/// </summary>
		/// <param name="hwnd">目标窗口句柄</param>
		/// <param name="style">要移除的样式</param>
		/// <returns></returns>
		bool RemoveWindowStyle(HWND Hwnd, LONG Style);
		/// <summary>
		/// 为指定窗口移除窗口样式
		/// </summary>
		/// <param name="window">目标窗口</param>
		/// <param name="style">要移除的样式</param>
		/// <returns></returns>
		bool RemoveWindowStyle(RenWin& Window, LONG Style);

		/// <summary>
		/// 为指定窗口移除拓展窗口样式
		/// </summary>
		/// <param name="hwnd">目标窗口句柄</param>
		/// <param name="style">要移除的样式</param>
		/// <returns></returns>
		bool RemoveWindowExStyle(HWND Hwnd, LONG Style);
		/// <summary>
		/// 为指定窗口移除拓展窗口样式
		/// </summary>
		/// <param name="window">目标窗口</param>
		/// <param name="style">要移除的样式</param>
		/// <returns></returns>
		bool RemoveWindowExStyle(RenWin& Window, LONG Style);

		/// <summary>
		/// 关闭窗口
		/// </summary>
		/// <param name="window">要关闭的窗口</param>
		void CloseWindow(RenWin& Window);

		/// <summary>
		/// 设置所属窗口
		/// </summary>
		/// <param name="window">父窗口</param>
		void SetOwnerWindow(RenWin& window);
		/// <summary>
		/// 设置所属窗口
		/// </summary>
		/// <param name="Hwnd">父窗口</param>
		void SetOwnerWindow(HWND Hwnd);

		/// <summary>
		/// 闪烁窗口用于焦点提示
		/// </summary>
		/// <param name="Window">目标窗口</param>
		void FlashWindow(RenWin& window,int Count = 5,const Color& color = Color::White);

		/// <summary>
		/// 设置窗口的透明度
		/// </summary>
		/// <param name="window">目标窗口</param>
		/// <param name="alpha">透明度 (0 - 255) </param>
		void SetWindowAlpha(RenWin& window, BYTE alpha);
		/// <summary>
		/// 设置窗口的透明度
		/// </summary>
		/// <param name="hwnd">目标窗口</param>
		/// <param name="alpha">透明度 (0 - 255) </param>
		void SetWindowAlpha(HWND hwnd, BYTE alpha);

		namespace DWM
		{
			/// <summary>
			/// 设置窗口圆角选项
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="round">圆角类型</param>
			void SetWindowRoundCorner(HWND Hwnd, WindowCorner Round);
			/// <summary>
			/// 设置窗口圆角选项
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="round">圆角类型</param>
			void SetWindowRoundCorner(RenWin& Window, WindowCorner Round);

			/// <summary>
			/// 设置窗口边框颜色
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="color">边框颜色</param>
			void SetWindowBorderColor(HWND Hwnd, Color Color);
			/// <summary>
			/// 设置窗口边框颜色
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="color">边框颜色</param>
			void SetWindowBorderColor(RenWin& Window, Color Color);

			/// <summary>
			/// 设置窗口标题栏颜色
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="color">标题栏颜色</param>
			void SetWindowTitleBarColor(HWND Hwnd, Color Color);
			/// <summary>
			/// 设置窗口标题栏颜色
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="color">标题栏颜色</param>
			void SetWindowTitleBarColor(RenWin& Window, Color Color);

			/// <summary>
			/// 设置窗口标题文字颜色
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="color">标题文字颜色</param>
			void SetWindowTitleTextColor(HWND Hwnd, Color Color);
			/// <summary>
			/// 设置窗口标题文字颜色
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="color">标题文字颜色</param>
			void SetWindowTitleTextColor(RenWin& Window, Color Color);

			/// <summary>
			/// 设置窗口背景类型
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="type">背景类型</param>
			void SetWindowBackType(HWND Hwnd, WindowBackType Type);
			/// <summary>
			/// 设置窗口背景类型
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="type">背景类型</param>
			void SetWindowBackType(RenWin& Window, WindowBackType Type);

			/// <summary>
			/// 设置窗口深色模式
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="EnableDarkMode">是否启用深色模式</param>
			void SetWindowDarkMode(HWND Hwnd, bool EnableDarkMode);
			/// <summary>
			/// 设置窗口深色模式
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="EnableDarkMode">是否启用深色模式</param>
			void SetWindowDarkMode(RenWin& Window, bool EnableDarkMode);

			/// <summary>
			/// 启用或禁用窗口阴影
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="enable">是否启用</param>
			void EnableWindowShadow(HWND Hwnd, bool enable);
			/// <summary>
			/// 启用或禁用窗口阴影
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="enable">是否启用</param>
			void EnableWindowShadow(RenWin& Window, bool enable);

			/// <summary>
			/// 扩展客户区到非客户区（用于自定义标题栏）
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="top">上边距</param>
			/// <param name="bottom">下边距</param>
			/// <param name="left">左边距</param>
			/// <param name="right">右边距</param>
			void ExtendIntoClientArea(HWND Hwnd, int top, int bottom, int left, int right);
			/// <summary>
			/// 扩展客户区到非客户区（用于自定义标题栏）
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="top">上边距</param>
			/// <param name="bottom">下边距</param>
			/// <param name="left">左边距</param>
			/// <param name="right">右边距</param>
			void ExtendIntoClientArea(RenWin& Window, int top, int bottom, int left, int right);

			/// <summary>
			/// 禁用窗口动画过渡效果
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="disable">是否禁用</param>
			void DisableTransitionAnimation(HWND Hwnd, bool disable);
			/// <summary>
			/// 禁用窗口动画过渡效果
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="disable">是否禁用</param>
			void DisableTransitionAnimation(RenWin& Window, bool disable);
		};
	};
}
