#pragma once
#include "Xs.Main.h"

namespace xs
{
	//虚拟键码
	enum VK
	{
		MouseLeft = 0x01,  // 鼠标左键
		MouseRight = 0x02,  // 鼠标右键
		MouseMiddle = 0x04,  // 鼠标中键

		Back = 0x08,  // 退格键
		Tab = 0x09,  // Tab 键
		Enter = 0x0d,  // 回车键

		Shift = 0x10,  // Shift 键
		Ctrl = 0x11,  // Ctrl 键

		Alt = 0x12,  // Alt 键
		Pause = 0x13,  // Pause 键

		CapsLock = 0x14,  // 大写锁定键
		Esc = 0x1b,  // Esc 键
		Space = 0x20,  // 空格键

		PageUp = 0x21,  // PgUp, 向上翻页
		PageDown = 0x22,  // PgDn, 向下翻页
		Home = 0x23,  // Home
		End = 0x24,  // End 

		Left = 0x25,  // ←, 左方向键
		Up = 0x26,  // ↑, 上方向键
		Right = 0x27,  // →, 右方向键
		Down = 0x28,  // ↓, 下方向键

		Print = 0x2a,  // Print, 打印键
		PrintScreen = 0x2c,  // Prt Sc, PrintScreen 截屏键

		Insert = 0x2d,  // Ins, 插入键
		Delete = 0x2e,  // Del, 删除键

		Big0 = 0x30, // 大键盘数字键0
		Big1 = 0x31, // 大键盘数字键1
		Big2 = 0x32, // 大键盘数字键2
		Big3 = 0x33, // 大键盘数字键3
		Big4 = 0x34, // 大键盘数字键4
		Big5 = 0x35, // 大键盘数字键5
		Big6 = 0x36, // 大键盘数字键6
		Big7 = 0x37, // 大键盘数字键7
		Big8 = 0x38, // 大键盘数字键8
		Big9 = 0x39, // 大键盘数字键9

		A = 0x41, // 字母键A
		B = 0x42,// 字母键B
		C = 0x43,// 字母键C
		D = 0x44,// 字母键D
		E = 0x45,// 字母键E
		F = 0x46,// 字母键F
		G = 0x47,// 字母键G
		H = 0x48,// 字母键H
		I = 0x49,// 字母键I
		J = 0x4a,// 字母键J
		K = 0x4b,// 字母键K
		L = 0x4c,// 字母键L
		M = 0x4d,// 字母键M
		N = 0x4e,// 字母键N
		O = 0x4f,// 字母键O
		P = 0x50,// 字母键P
		Q = 0x51,// 字母键Q
		R = 0x52,// 字母键R
		S = 0x53,// 字母键S
		T = 0x54,// 字母键T
		U = 0x55,// 字母键U
		V = 0x56,// 字母键V
		W = 0x57,// 字母键W
		X = 0x58,// 字母键X
		Y = 0x59,// 字母键Y
		Z = 0x5a,// 字母键Z

		WinLeft = 0x5b,  // 左 Windows 徽标键
		WinRight = 0x5c,  // 右 Windows 徽标键

		Sleep = 0x5f,  // 休眠键

		Min0 = 0x60,//九宫格小键盘0
		Min1 = 0x61,//九宫格小键盘1
		Min2 = 0x62,//九宫格小键盘2
		Min3 = 0x63,//九宫格小键盘3
		Min4 = 0x64,//九宫格小键盘4
		Min5 = 0x65,//九宫格小键盘5
		Min6 = 0x66,//九宫格小键盘6
		Min7 = 0x67,//九宫格小键盘7
		Min8 = 0x68,//九宫格小键盘8
		Min9 = 0x69,//九宫格小键盘9

		MinMultiply = 0x6a,  // *，乘号键
		MinAdd = 0x6b,  // +, 加号键
		MinSeparator = 0x6c,  //    分割键
		MinSubtract = 0x6d,  // -, 减号键
		MinDecimal = 0x6e,  // ., 小数点
		MinDivide = 0x6f,  // /, 除号键


		Fn1 = 0x70,//Fn1
		Fn2 = 0x71,//Fn2
		Fn3 = 0x72,//Fn3
		Fn4 = 0x73,//Fn4
		Fn5 = 0x74,//Fn5
		Fn6 = 0x75,//Fn6
		Fn7 = 0x76,//Fn7
		Fn8 = 0x77,//Fn8
		Fn9 = 0x78,//Fn9
		Fn10 = 0x79,//Fn0
		Fn11 = 0x7a,//Fn11
		Fn12 = 0x7b,//Fn12

		MinNumlock = 0x90,  // NumLk, 小键盘数字锁定

		ScrollLock = 0x91,  // ScrLk, 滚动锁定键

		ShiftL = 0xa0,  // 左 Shift
		ShiftR = 0xa1,  // 右 Shift
		CtrlL = 0xa2,  // 左 Ctrl
		CtrlR = 0xa3,  // 右 Ctrl
		AltL = 0xa4,  // 左 Alt
		AltR = 0xa5,  // 右 Alt

		// 大键盘上的符号键
		Semicolon = 0xba,  // ; 分号键
		Plus = 0xbb,  // + 加号键
		Comma = 0xbc,  // , 逗号键
		Minus = 0xbd,  // - 减号键
		Period = 0xbe,  // . 句号键
		Slash = 0xbf,  // / 右斜杠键
		Tilde = 0xc0,  // ~ 波浪键
		Lbrace = 0xdb,  // [ 左方括号键
		Backslash = 0xdc,  // \ 反斜杠键
		Rbrace = 0xdd,  // ] 右方括号键
		Quote = 0xde,  // ' 引号键
	};
	//消息
	namespace XMsg
	{

		/// <summary>
		/// 更新窗口消息
		/// </summary>
		/// <param name="Dest">目标窗口</param>
		void UpdateMsg(RenWin& Dest);
		/// <summary>
		/// 判断当前窗口的关闭消息
		/// </summary>
		/// <returns></returns>
		bool IsClose(RenWin& window);

		/// <summary>
		/// 检测窗口是否运行，等价于!IsClose(RenWin& window);
		/// </summary>
		/// <param name="window">目标窗口</param>
		/// <returns>窗口是否在运行</returns>
		bool IsOpen(RenWin& window);

		/// <summary>
		/// 重设置关闭消息状态，调用后IsClose将返回false
		/// </summary>
		void ResetCloseMsg();
		/// <summary>
		/// 窗口消息类
		/// </summary>
		namespace WindowMsg
		{

			bool IsWindowMove();
			bool IsWindowResize();
			bool IsWindowNeedClose();
			bool IsWindowMin();
			bool IsWindowMax();
		};
		/// <summary>
		/// 鼠标消息类
		/// </summary>
		namespace MouseMsg
		{

			/// <summary>
			/// 获取鼠标在指定窗口上的位置
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <returns></returns>
			Vector2i GetMousePosWindow();
			/// <summary>
			/// 获取归一化的位置
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="x">基准坐标x</param>
			/// <param name="y">基准坐标y</param>
			/// <param name="w">基准宽</param>
			/// <param name="h">基准高</param>
			/// <returns>返回归一化坐标</returns>
			Vector2f GetMousePosWindowNormalization(int x = -1, int y = -1, int w = -1, int h = -1);
			/// <summary>
			/// 获取鼠标在屏幕上的位置
			/// </summary>
			/// <returns></returns>
			Vector2i GetMousePosScreen();
			/// <summary>
			/// 获取鼠标在屏幕上归一化的位置
			/// </summary>
			/// <param name="x">基准坐标x</param>
			/// <param name="y">基准坐标y</param>
			/// <param name="w">基准宽</param>
			/// <param name="h">基准高</param>
			/// <returns>返回归一化坐标</returns>
			Vector2f GetMousePosScreenNormalization(int x = -1, int y = -1, int w = -1, int h = -1);
			/// <summary>
			/// 获取鼠标的滚轮值，返回120的倍数
			/// </summary>
			/// <returns></returns>
			int GetMouseWheel();

			/// <summary>
			/// 判断当前鼠标是否被按下
			/// </summary>
			/// <param name="Dest">目标窗口</param>
			/// <param name="button">需要判断的鼠标值，位于VK中</param>
			/// <param name="id">目标ID，具有默认值，可以不填</param>
			/// <returns></returns>
			bool IsMouseDown(VK button);
			/// <summary>
			/// 判断鼠标是否在窗口的指定区域内
			/// </summary>
			/// <param name="x">指定区域的左上角坐标x</param>
			/// <param name="y">指定区域的左上角坐标y</param>
			/// <param name="w">指定区域的宽</param>
			/// <param name="h">指定区域的高</param>
			/// <param name="Dest">目标窗口</param>
			/// <param name="id">目标ID，具有默认值，可以不填</param>
			/// <returns></returns>
			bool IsMouseIn(int x, int y, int w, int h);
			/// <summary>
			/// 判断当前鼠标是否处于按下状态
			/// </summary>
			/// <param name="Dest">目标窗口</param>
			/// <param name="button">需要判断的鼠标值，位于VK中</param>
			/// <param name="id">目标ID，具有默认值，可以不填</param>
			/// <returns></returns>
			bool IsMousePress(VK button);
			/// <summary>
			/// 判断当前鼠标是否滚轮
			/// </summary>
			/// <returns></returns>
			bool IsWheel();
			/// <summary>
			/// 判断当前鼠标是否移动
			/// </summary>
			/// <param name="id"></param>
			/// <returns></returns>
			bool IsMouseMove();
			/// <summary>
			/// 设置鼠标禁用区域，这将会导致这一帧内所有在此区域内的鼠标消息失灵
			/// </summary>
			/// <param name="x">区域x坐标</param>
			/// <param name="y">区域y坐标</param>
			/// <param name="w">区域宽</param>
			/// <param name="h">区域高</param>
			void SetBanZone(int x, int y, int w, int h);
		};
		/// <summary>
		/// 键盘消息类
		/// </summary>
		namespace KeyMsg
		{

			/// <summary>
			/// 判断虚拟键码对应的按键是否被按下.
			/// </summary>
			/// <param name="VK">虚拟键码</param>
			/// <returns></returns>
			bool Keystate(VK VK);
			/// <summary>
			/// 判断虚拟键码对应的按键是否具有消息，VK值请参照虚拟键码
			/// </summary>
			/// <param name="VK">虚拟键码</param>
			/// <returns></returns>
			bool Keystate(int VK);
			/// <summary>
			/// 检查当前按键是否按下
			/// </summary>
			/// <param name="VK">虚拟键码</param>
			/// <returns></returns>
			bool IsKeyDown(VK VK);
			/// <summary>
			/// 检查当前按键是否按下
			/// </summary>
			/// <param name="VK">虚拟键码</param>
			/// <returns></returns>
			bool IsKeyDown(int VK);
			/// <summary>
			/// 检查当前按键是否松开
			/// </summary>
			/// <param name="VK">虚拟键码</param>
			bool IsKeyReleased(VK VK);
			/// <summary>
			/// 检查当前按键是否松开
			/// </summary>
			/// <param name="VK">虚拟键码</param>
			bool IsKeyReleased(int VK);
			/// <summary>
			/// 获取输入的字符
			/// </summary>
			/// <returns>有则返回，无则返回空值，/b退格，/n回车</returns>
			std::string GetInPut();
			/// <summary>
			/// 允许合法的数字输入，str为当前输入的字符串，函数会在其基础上追加合法输入并返回，若输入不合法则返回原字符串
			/// </summary>
			/// <param name="str">当前输入的字符串</param>
			/// <returns>合法数字字符串</returns>
			std::string GetInPutNumber(std::string str);
			/// <summary>
			/// 设置IME候选窗口的位置，hwnd为目标窗口句柄，x和y为候选窗口的屏幕坐标
			/// </summary>
			/// <param name="hwnd">目标窗口句柄</param>
			/// <param name="x">候选窗口的屏幕坐标X</param>
			/// <param name="y">候选窗口的屏幕坐标Y</param>
			void SetIMEPos(int x, int y);
			/// <summary>
			/// 热键管理类
			/// </summary>
			namespace HotKey
			{

				/// <summary>
				/// 注册热键，如果存在则更新
				/// </summary>
				/// <param name="name">热键标识</param>
				/// <param name="VK">热键值</param>
				void RegisterHotKey(const std::string& name, std::vector<VK> VK);
				/// <summary>
				/// 判断热键是否按下
				/// </summary>
				/// <param name="name">热键标识</param>
				/// <returns>bool值，按下返回true，否则返回false</returns>
				bool IsHotKeyPressed(const std::string& name);
				/// <summary>
				/// 卸载热键
				/// </summary>
				/// <param name="name">热键标识</param>
				void UnRegisterHotKey(const std::string& name);
			};
		};
		/// <summary>
		/// 触控消息类
		/// </summary>
		namespace TouchMsg
		{

			bool IsTouching();
			int GetTouchNum();
			Vector2i GetTouchPos();
			int GetTouchSize();
			int GetTouchWheel();
		};
		/// <summary>
		/// 复合函数类
		/// </summary>
		namespace Composite
		{

			/// <summary>
			/// 检查当前鼠标是否在指定区域按下
			/// </summary>
			/// <param name="VK">虚拟键码</param>
			/// <param name="x">坐标x</param>
			/// <param name="y">坐标y</param>
			/// <param name="w">坐标w</param>
			/// <param name="h">坐标h</param>
			/// <param name="window">目标窗口</param>
			/// <returns>如果按下返回true</returns>
			bool IsMouseInRectDown(VK VK, int x, int y, int w, int h);
			/// <summary>
			/// 获取鼠标在指定区域的滚轮值
			/// </summary>
			/// <param name="x">坐标x</param>
			/// <param name="y">坐标y</param>
			/// <param name="w">区域宽</param>
			/// <param name="h">区域高</param>
			/// <param name="window">目标窗口</param>
			/// <returns>返回滚轮值，为120的整数</returns>
			int GetMouseRectWheel(int x, int y, int w, int h);
		};

		/// <summary>
		/// 设置消息睡眠时间，单位毫秒。
		/// </summary>
		/// <param name="time"></param>
		void SetSleepTime(int time);
		/// <summary>
		/// 清理消息缓冲区
		/// </summary>
		/// <param name="id"></param>
		void ClearMsg();
	};
}


