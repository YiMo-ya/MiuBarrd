#include "SoundPlayer.h"

NOSTD;

void WavPlayer::Load(const std::wstring& name, const std::wstring& path)
{
	// 先关掉旧的（如果有）
	mciSendStringW((L"close " + name).c_str(), NULL, 0, NULL);

	// 预打开
	std::wstring cmd = L"open \"" + path + L"\" type waveaudio alias " + name;
	mciSendStringW(cmd.c_str(), NULL, 0, NULL);

	// 设音量
	std::wstring vol = L"setaudio " + name + L" volume to 100";
	mciSendStringW(vol.c_str(), NULL, 0, NULL);
}

void WavPlayer::Play(const std::wstring& name)
{
	// 从头播，不重新 open → 零延迟
	mciSendStringW((L"seek " + name + L" to start").c_str(), NULL, 0, NULL);
	mciSendStringW((L"play " + name).c_str(), NULL, 0, NULL);
}

void WavPlayer::Stop(const std::wstring& name)
{
	mciSendStringW((L"stop " + name).c_str(), NULL, 0, NULL);
}

WavPlayer player;

