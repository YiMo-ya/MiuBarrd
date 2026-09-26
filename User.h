#pragma once
#include "Xs/Xs.h"

class User
{
public:
	static Color MainColor;

	static bool EnableFixShape;
	static bool EnableWriteAdjust;

	static void Read();

	//保存配置到 User.ini
	static void Save();
};


