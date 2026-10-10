#include "User.h"
#include "Shared.h"
#include "BottomMessage.h"

NOSTD; NOXS;

Color User::MainColor = Color(190, 170, 255);
Color User::BackColor = Color(33, 33, 33);
bool User::EnableFixShape = true;
bool User::EnableWriteAdjust = true;
bool User::EnableAnimation = true;
bool User::EnableExpTool = true;
bool User::EnableBack = true;

void User::Read()
{
	ifstream UserLoad("User.ini");
	if (UserLoad.is_open())
	{
		while (1)
		{
			string temp;
			UserLoad >> temp;
			if (temp.empty()) break;

			if (temp == "[MAINCOLOR]")
			{
				int r, g, b;

				UserLoad >> r >> g >> b;
				MainColor = Color(r, g, b);
			}
			if (temp == "[BACKCOLOR]")
			{
				int r, g, b;

				UserLoad >> r >> g >> b;
				BackColor = Color(r, g, b);
			}
			if (temp == "[FIXSHAPE]")
			{
				UserLoad >> EnableFixShape;
			}
			if (temp == "[ADJUST]")
			{
				UserLoad >> EnableWriteAdjust;
			}
			if (temp == "[ANIMATION]")
			{
				UserLoad >> EnableAnimation;
			}
			if (temp == "[EXP]")
			{
				UserLoad >> EnableExpTool;
			}
			if (temp == "[BACK]")
			{
				UserLoad >> EnableBack;
			}
		}
	}
	else
	{
		UserLoad.close();

		ofstream UserSave("User.ini");
		if (!UserSave.is_open()) return;

		UserSave << "[MAINCOLOR] " << to_string(MainColor.r) << " " << to_string(MainColor.g) << " " << to_string(MainColor.b) << endl
			<< "[BackColor] " << to_string(BackColor.r) << " " << to_string(BackColor.g) << " " << to_string(BackColor.b) << endl
			<< "[FIXSHAPE] " << to_string(EnableFixShape ? 1 : 0) << endl
			<< "[ADJUST] " << to_string(EnableWriteAdjust ? 1 : 0) << endl
			<< "[ANIMATION] " << to_string(EnableAnimation ? 1 : 0) << endl
			<< "[EXP] " << to_string(EnableExpTool ? 1 : 0) << endl
			<< "[BACK] " << to_string(EnableBack ? 1 : 0) << endl;

		UserSave.close();
	}
}

void User::Save()
{
	ofstream UserSave("User.ini");
	if (!UserSave.is_open()) return;

	UserSave << "[MAINCOLOR] " << to_string(MainColor.r) << " " << to_string(MainColor.g) << " " << to_string(MainColor.b) << endl
		<< "[FIXSHAPE] " << to_string(EnableFixShape ? 1 : 0) << endl
		<< "[ADJUST] " << to_string(EnableWriteAdjust ? 1 : 0) << endl
		<< "[ANIMATION] " << to_string(EnableAnimation ? 1 : 0) << endl
		<< "[EXP] " << to_string(EnableExpTool ? 1 : 0) << endl;;

	UserSave.close();

	UpdateUser = 2;

	BottomMessage::AddMessage(200, L"已更新设置", BottomMessageType_SUCCESS, 120);
}