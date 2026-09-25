#pragma once
#include "Xs/Xs.h"

enum RightMessageType
{
	RightMessageType_INFO, RightMessageType_ERROR, RightMessageType_SUCCESS, RightMessageType_WARNING
};

class RightMessage
{
public:
	static void ShowMessage(std::wstring text, std::wstring title, RightMessageType type, bool important = false);
	static void Manage(RenWin& window);

	static void SetVisible(bool v);
};