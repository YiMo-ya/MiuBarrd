#pragma once

#pragma warning(disable: 4996 4244 4819 4828 4834 4819 4305)


#ifndef M_PI

#define M_PI 3.14159265358979323846

#endif



#include <Windows.h>
#include <string>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <dwmapi.h>
#include <math.h>
#pragma comment(lib, "dwmapi.lib")
#include <thread>
#include <map>
#include <unordered_map>
#include <imm.h>
#pragma comment(lib, "imm32.lib")

#ifdef STATIC

#define SFML_STATIC

#include "SFML/include/SFML/Graphics.hpp"
#include "SFML/include/SFML/Window.hpp"
#include "SFML/include/SFML/System.hpp"
#include "SFML/include/SFML/Audio.hpp"
#include "SFML/include/SFML/Network.hpp"

// ── 静态链接 SFML ──
#pragma comment(lib, "Xs/sfml-graphics-s.lib")
#pragma comment(lib, "Xs/sfml-window-s.lib")
#pragma comment(lib, "Xs/sfml-system-s.lib")
#pragma comment(lib, "Xs/sfml-audio-s.lib")
#pragma comment(lib, "Xs/sfml-network-s.lib")

#else

#include "SFML/include/SFML/Graphics.hpp"
#include "SFML/include/SFML/Window.hpp"
#include "SFML/include/SFML/System.hpp"
#include "SFML/include/SFML/Audio.hpp"
#include "SFML/include/SFML/Network.hpp"

// ── 动态链接 SFML ──
#pragma comment(lib, "Xs/sfml-graphics.lib")
#pragma comment(lib, "Xs/sfml-window.lib")
#pragma comment(lib, "Xs/sfml-system.lib")
#pragma comment(lib, "Xs/sfml-audio.lib")
#pragma comment(lib, "Xs/sfml-network.lib")

#endif

// 公共依赖
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Xs/freetype.lib")
#pragma comment(lib, "Xs/harfbuzz.lib")
#pragma comment(lib, "Xs/flac.lib")
#pragma comment(lib, "Xs/ogg.lib")
#pragma comment(lib, "Xs/vorbis.lib")
#pragma comment(lib, "Xs/vorbisfile.lib")

//全局命名空间定义
using namespace sf;

using RenWin = RenderWindow;
using Rt = RenderTarget;
using Rtt = RenderTexture;

//托盘消息
#define WM_TRAY_CALLBACK (WM_USER + 1)