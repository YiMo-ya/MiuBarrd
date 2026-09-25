#pragma once
#include "Xs/Xs.h"

#include <mmsystem.h>

class WavPlayer
{
public:
	void Load(const std::wstring& name, const std::wstring& path);

	void Play(const std::wstring& name);

	void Stop(const std::wstring& name);
};
extern WavPlayer player;