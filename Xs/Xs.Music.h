#pragma once
#include "Xs.Main.h"

namespace xs
{
	struct MusicRuntime
	{
		float fadeTimer = 0.f;
		float fadeFrom = 0.f;
		float fadeTo = 0.f;
		float fadeDuration = 0.f;
		bool fading = false;
	};

	struct SoundRuntime
	{
		float fadeTimer = 0.f;
		float fadeFrom = 0.f;
		float fadeTo = 0.f;
		float fadeDuration = 0.f;
		bool fading = false;
	};

	//音频，从文件中直接读取
	struct music
	{
		//音频
		Music music;

		//音量，默认100%
		float volume = 100.0f;

		//淡入淡出
		struct Fade
		{
			//淡入时长
			float FadeStart;
			//淡出时长
			float FadeEnd;
		};

		//总时长
		float AllTime;

		~music();
	};
	using MUSIC = music;

	//音频，加载到内存后读取，只播放一次
	struct sound
	{
		//音频
		Sound sound;

		//音量，默认100%
		float volume = 100.0f;

		//淡入淡出
		struct Fade
		{
			//淡入时长
			float FadeStart;
			//淡出时长
			float FadeEnd;
		};

		//总时长
		float AllTime;

		bool loaded = false;

		~sound();
	};
	using SOUND = sound;

	extern std::unordered_map<MUSIC*, MusicRuntime> g_MusicRuntime;
	extern std::unordered_map<SOUND*, SoundRuntime> g_SoundRuntime;

	enum PlayStatus
	{
		//停止
		STOP = 0,
		//播放
		PLAY = 1,
		//暂停
		PAUSE = 2
	};

	namespace XMusic
	{
		/// <summary>
		/// 更新音频播放状态，必须在每帧调用，内部函数
		/// </summary>
		void Update();
		/// <summary>
		/// 加载音频到MUSIC中
		/// </summary>
		/// <param name="music">要加载的目标MUSIC</param>
		/// <param name="path">音频文件路径</param>
		/// <returns>加载成功返回true，否则返回false</returns>
		bool Load(MUSIC& music, std::wstring path);
		/// <summary>
		/// 加载音频到MUSIC中
		/// </summary>
		/// <param name="music">要加载的目标MUSIC</param>
		/// <param name="path">音频文件路径</param>
		/// <returns>加载成功返回true，否则返回false</returns>
		bool Load(MUSIC& music,std::string path);
		/// <summary>
		/// 加载音频到SOUND中
		/// </summary>
		/// <param name="sound">要加载的目标SOUND</param>
		/// <param name="path">音频文件路径</param>
		/// <returns>加载成功返回true，否则返回false</returns>
		bool Load(SOUND& sound, std::wstring path);
		/// <summary>
		/// 加载音频到SOUND中
		/// </summary>
		/// <param name="sound">要加载的目标SOUND</param>
		/// <param name="path">音频文件路径</param>
		/// <returns>加载成功返回true，否则返回false</returns>
		bool Load(SOUND& sound, std::string path);
		/// <summary>
		/// 播放音频
		/// </summary>
		/// <param name="music">要播放的音频</param>
		/// <param name="loop">是否循环播放</param>
		void Play(MUSIC& music, bool loop = false);
		/// <summary>
		/// 播放音频
		/// </summary>
		/// <param name="sound">要播放的音频</param>
		/// <param name="loop">是否循环播放</param>
		void Play(SOUND& sound, bool loop = false);
		/// <summary>
		/// 暂停音频
		/// </summary>
		/// <param name="music">要暂停的音频</param>
		void Pause(MUSIC& music);
		/// <summary>
		/// 暂停音频
		/// </summary>
		/// <param name="sound">要暂停的音频</param>
		void Pause(SOUND& sound);
		/// <summary>
		/// 停止音频
		/// </summary>
		/// <param name="music">要停止的音频</param>
		void Stop(MUSIC& music);
		/// <summary>
		/// 停止音频
		/// </summary>
		/// <param name="sound">要停止的音频</param>
		void Stop(SOUND& sound);
		/// <summary>
		/// 设置音量
		/// </summary>
		/// <param name="music">要设置音量的音频</param>
		/// <param name="volume">音量值</param>
		void SetVolume(MUSIC& music, float volume);
		/// <summary>
		/// 设置音量
		/// </summary>
		/// <param name="sound">要设置音量的音频</param>
		/// <param name="volume">音量值</param>
		void SetVolume(SOUND& sound, float volume);
		/// <summary>
		/// 设置循环
		/// </summary>
		/// <param name="music">要设置循环的音频</param>
		/// <param name="loop">是否循环</param>
		void SetLoop(MUSIC& music, bool loop);
		/// <summary>
		/// 设置淡入淡出
		/// </summary>
		/// <param name="music">要设置淡入淡出的音频</param>
		/// <param name="fadeStart">淡入时长</param>
		/// <param name="fadeEnd">淡出时长</param>
		void SetFade(MUSIC& music, float fadeStart, float fadeEnd);
		/// <summary>
		/// 设置淡入淡出
		/// </summary>
		/// <param name="sound">要设置淡入淡出的音频</param>
		/// <param name="fadeStart">淡入时长</param>
		/// <param name="fadeEnd">淡出时长</param>
		void SetFade(SOUND& sound, float fadeStart, float fadeEnd);
		/// <summary>
		/// 跳转播放位置
		/// </summary>
		/// <param name="music">要跳转播放位置的音频</param>
		/// <param name="offset">播放位置偏移量</param>
		void SetOffset(MUSIC& music, float offset);
		/// <summary>
		/// 跳转播放位置
		/// </summary>
		/// <param name="sound">要跳转播放位置的音频</param>
		/// <param name="offset">播放位置偏移量</param>
		void SetOffset(SOUND& sound, float offset);
		/// <summary>
		/// 获取音频播放状态
		/// </summary>
		/// <param name="music">要获取播放状态的音频</param>
		/// <returns>返回播放状态</returns>
		PlayStatus GetPlayStatue(MUSIC& music);
		/// <summary>
		/// 获取音频播放状态
		/// </summary>
		/// <param name="sound">要获取播放状态的音频</param>
		/// <returns>返回播放状态</returns>
		PlayStatus GetPlayStatue(SOUND& sound);
		/// <summary>
		/// 设置音调
		/// </summary>
		/// <param name="music">要设置音调的音频</param>
		/// <param name="pitch">音调值，注意：会受到硬件限制</param>
		void SetPitch(MUSIC& music, float pitch);
		/// <summary>
		/// 设置音调
		/// </summary>
		/// <param name="sound">要设置音调的音频</param>
		/// <param name="pitch">音调值，注意：会受到硬件限制</param>
		void SetPitch(SOUND& sound, float pitch);
		/// <summary>
		/// 设置音频循环播放的时间段
		/// </summary>
		/// <param name="music">要设置循环播放时间的音频</param>
		/// <param name="Start">循环开始时间</param>
		/// <param name="End">循环结束时间</param>
		void SetLoopTime(MUSIC& music, float Start,float End);
		/// <summary>
		/// 获取音量
		/// </summary>
		/// <param name="music">要获取音量的音频</param>
		/// <returns>返回音量值</returns>
		float GetVolume(MUSIC& music);
		/// <summary>
		/// 获取音量
		/// </summary>
		/// <param name="sound">要获取音量的音频</param>
		/// <returns>返回音量值</returns>
		float GetVolume(SOUND& sound);

		namespace Audio3D
		{
			/// <summary>
			/// 设置3D音效
			/// </summary>
			/// <param name="music">要设置3D音效的音频</param>
			/// <param name="x">X坐标</param>
			/// <param name="y">Y坐标</param>
			/// <param name="z">Z坐标</param>
			/// <param name="relative">3D音效相对于监听器的位置，true为相对于监听器，false为世界位置</param>
			void Set3DPosition(MUSIC& music, float x, float y, float z, bool relative);
			/// <summary>
			/// 设置3D音效
			/// </summary>
			/// <param name="sound">要设置3D音效的音频</param>
			/// <param name="x">X坐标</param>
			/// <param name="y">Y坐标</param>
			/// <param name="z">Z坐标</param>
			/// <param name="relative">3D音效相对于监听器的位置，true为相对于监听器，false为世界位置</param>
			void Set3DPosition(SOUND& sound, float x, float y, float z, bool relative);
			/// <summary>
			/// 获取3D音效位置
			/// </summary>
			/// <param name="music">要获取3D音效位置的音频</param>
			/// <returns>返回3D音效位置</returns>
			Vector2f Get3DPosition(MUSIC& music);
			/// <summary>
			/// 获取3D音效位置
			/// </summary>
			/// <param name="sound">要获取3D音效位置的音频</param>
			/// <returns>返回3D音效位置</returns>
			Vector2f Get3DPosition(SOUND& sound);
			/// <summary>
			/// 设置音量保持最大的最小距离
			/// </summary>
			/// <param name="music">要设置最小距离的音频</param>
			/// <param name="minDistance">最小距离</param>
			void SetMinDistance(MUSIC& music, float minDistance);
			/// <summary>
			/// 设置音量保持最大的最小距离
			/// </summary>
			/// <param name="sound">要设置最小距离的音频</param>
			/// <param name="minDistance">最小距离</param>
			void SetMinDistance(SOUND& sound, float minDistance);
			/// <summary>
			/// 启用或禁用3D音效
			/// </summary>
			/// <param name="music">要启用或禁用3D音效的音频</param>
			/// <param name="enable">true为启用，false为禁用</param>
			void Enable3D(MUSIC& music, bool enable);
			/// <summary>
			/// 启用或禁用3D音效
			/// </summary>
			/// <param name="sound">要启用或禁用3D音效的音频</param>
			/// <param name="enable">true为启用，false为禁用</param>
			void Enable3D(SOUND& sound, bool enable);
			/// <summary>
			/// 设置音量衰减
			/// </summary>
			/// <param name="music">要设置音量衰减的音频</param>
			/// <param name="attenuation">衰减值</param>
			void SetAttenuation(MUSIC& music, float attenuation);
			/// <summary>
			/// 设置音量衰减
			/// </summary>
			/// <param name="sound">要设置音量衰减的音频</param>
			/// <param name="attenuation">衰减值</param>
			void SetAttenuation(SOUND& sound, float attenuation);
			/// <summary>
			/// 获取衰减后的实际音量值
			/// </summary>
			/// <param name="music">要获取衰减后音量的音频</param>
			/// <returns>返回衰减后的实际音量值</returns>
			float GetTrueVolume(MUSIC& music);
			/// <summary>
			/// 获取衰减后的实际音量值
			/// </summary>
			/// <param name="sound">要获取衰减后音量的音频</param>
			/// <returns>返回衰减后的实际音量值</returns>
			float GetTrueVolume(SOUND& sound);
		};
	};
};