#pragma once
#include "Xs/Xs.h"

//颜色选择窗口（取消时返回 cancelColor）
Color ChooseColorWindow(const Color& cancelColor, RenderWindow& window);

class Tool

{
public:

	//工具序号
	static int ToolCount;

	//笔帽
	static int PenCap;
	//笔颜色
	static Color PenColor;
	//笔大小
	static int PenSize;

	static bool IsInBar;

	static void Draw(RenWin& window);

	static void HideMoreBar();

	static void UpdatePage();

	static void UpdateCameraControl();

	static void ShowPhotoWindow();

	class Exp
	{
	public:
		static void ExpBottomToolBar();

		static void ExpStartBar();
		static void ExpMinBar();

		static void ExpPageBar();

		static void ExpLeftScrollBar();
		static void ExpRightScrollBar();
	};
};