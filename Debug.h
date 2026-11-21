#pragma once
#include "Xs/Xs.h"

extern bool Debug;

class DebugClass
{
public:
	static void Draw(RenWin& window);

	static void PushMsg(std::wstring msg);
};