#include "Shared.h"

Vector2i WindowSize, ScreenSize;

float ScreenScale;

int FONTSIZE;

std::wstring ImgPath = std::filesystem::current_path().wstring() + L"\\Image\\";

std::wstring ShaderPath = std::filesystem::current_path().wstring() + L"\\Shader\\";

int UpdateUser = 0;

std::string VER = "6.2.0_Beta7";