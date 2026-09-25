#pragma once
#include "Xs/Xs.h"

enum IconType
{
	ICOTYPE_DEBUG = 0,
	ICOTYPE_INFO = 1,
	ICOTYPE_QUESTION = 2,
	ICOTYPE_ERROR = 3,
	ICOTYPE_SUCCESS = 4,
	ICOTYPE_WARNING = 5
};

struct MsgS
{
	std::string text, title;
	IconType Ico;

	std::vector<std::string> buttons;

	int audio = 0;//0无声，1默认声音，2警告声音，3错误声音
};

class Message
{
public:
	static int ShowMessage(MsgS msg,std::wstring WindowTitle = L"NULL");
	static int ShowMessage(std::string text, std::string title = "",
		IconType ico = ICOTYPE_INFO,
		std::vector<std::string> button = { "确定" },
		int audio = 1,std::wstring WindowTitle = L"NULL");
};