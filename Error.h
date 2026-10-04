#pragma once
#include "Xs/Xs.h"

class Error
{
public:
	static void ShowError(RenWin& window,std::vector<std::wstring> Discription, std::wstring ErrorCode);
};