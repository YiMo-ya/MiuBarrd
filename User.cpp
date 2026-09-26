#include "User.h"
#include "Shared.h"
#include "BottomMessage.h"

NOSTD; NOXS;

Color User::MainColor = Color(180, 200, 255);
bool User::EnableFixShape = true;
bool User::EnableWriteAdjust = true;

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

			if (temp == "{MainColor}")
			{
				int r, g, b;

				UserLoad >> r >> g >> b;
				MainColor = Color(r, g, b);
			}
			if (temp == "{FIXLINE}")
			{
				UserLoad >> EnableFixShape;
			}
			if (temp == "{ADJUST}")
			{
				UserLoad >> EnableWriteAdjust;
			}
		}
	}
	else
	{
		UserLoad.close();

		ofstream ReUser("User.ini");

		ReUser << "{MainColor} 180 200 255" << endl << "{FXAA} 2" << "{FIXLINE} 1" << "{ADJUST} 1";

		ReUser.close();
	}
}

void User::Save()
{
	ofstream UserSave("User.ini");
	if (!UserSave.is_open()) return;

	UserSave << "{MainColor} " << to_string(MainColor.r) << " " << to_string(MainColor.g) << " " << to_string(MainColor.b) << endl
		<< "{FIXLINE} " << to_string(EnableFixShape ? 1 : 0) << endl
		<< "{ADJUST} " << to_string(EnableWriteAdjust ? 1 : 0) << endl;

	UserSave.close();

	UpdateUser = 2;

	BottomMessage::AddMessage(200, L"已更新设置", BottomMessageType_SUCCESS, 120);
}