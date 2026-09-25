#pragma once

#include "Xs/Xs.h"

//设置面板
//是否应该进入设置界面
extern bool NeedEnterSetting;

//进入设置界面（阻塞式，直到用户关闭）
void EnterSetting(RenWin& window);
