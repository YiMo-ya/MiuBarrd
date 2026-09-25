#pragma once
#include "Xs/Xs.h"

enum BottomMessageType
{
	BottomMessageType_LOAD,
	BottomMessageType_ERROR,
	BottomMessageType_SUCCESS,
	BottomMessageType_INFO
};

class BottomMessage
{
public:
	static void AddMessage(int id,std::wstring text, BottomMessageType type,int time = 300);
	static void UpdateCurrentMessageText(std::wstring text);
	static void UpdateCurretnMessageTime(int time = 300);
	static void UpdateCurrentMessageType(BottomMessageType type);
	static bool IsMessageNow(int id);
	static void ResetY();

	static void MessageManage(RenWin& window);

	static void SetYOffset(int value);
};