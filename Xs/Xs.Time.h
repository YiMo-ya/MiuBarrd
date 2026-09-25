#pragma once
#include "Xs.Main.h"

extern void xtimecounter();

namespace xs
{

namespace XTime
{
	/// <summary>
	/// 设置计时器
	/// </summary>
	/// <param name="name">计时器名字</param>
	void SetTimer(std::string name);
	/// <summary>
	/// 设置计时器
	/// </summary>
	/// <param name="name">计时器名字</param>
	void SetTimer(std::wstring name);
	/// <summary>
	/// 获取计时器的计时
	/// </summary>
	/// <param name="name">计时器名字</param>
	/// <returns>计数的帧数</returns>
	int GetTime(std::string name);
	/// <summary>
	/// 获取计时器的计时
	/// </summary>
	/// <param name="name">计时器名字</param>
	/// <returns>计数的帧数</returns>
	int GetTime(std::wstring name);

	/// <summary>
	/// 开始计时
	/// </summary>
	/// <param name="name">计时器名字</param>
	void StartTimer(std::string name);
	/// <summary>
	/// 开始计时
	/// </summary>
	/// <param name="name">计时器名字</param>
	void StartTimer(std::wstring name);
	/// <summary>
	/// 暂停计时
	/// </summary>
	/// <param name="name">计时器名字</param>
	void StopTimer(std::string name);
	/// <summary>
	/// 暂停计时
	/// </summary>
	/// <param name="name">计时器名字</param>
	void StopTimer(std::wstring name);

	/// <summary>
	/// 删除计时器
	/// </summary>
	/// <param name="name">计时器名字</param>
	/// <param name="注意">智能删除暂停的计时器</param>
	void RemoveTimer(std::string name);
	/// <summary>
	/// 删除计时器
	/// </summary>
	/// <param name="name">计时器名字</param>
	/// <param name="注意">智能删除暂停的计时器</param>
	void RemoveTimer(std::wstring name);

	/// <summary>
	/// 获取当前时间年份
	/// </summary>
	/// <returns>当前年份</returns>
	int GetTimeNow_Year();
	/// <summary>
	/// 获取当前时间月份
	/// </summary>
	/// <returns>当前月份</returns>
	int GetTimeNow_Month();
	/// <summary>
	/// 获取当前日期
	/// </summary>
	/// <returns>当前日期</returns>
	int GetTimeNow_Day();
	/// <summary>
	/// 获取当前周
	/// </summary>
	/// <returns>当前周：从0(周日) 开始</returns>
	int GetTimeNow_Week();
	/// <summary>
	/// 获取当前小时
	/// </summary>
	/// <returns>当前小时</returns>
	int GetTimeNow_Hour();
	/// <summary>
	/// 获取当前分
	/// </summary>
	/// <returns>当前分</returns>
	int GetTimeNow_Min();
	/// <summary>
	/// 获取当前秒
	/// </summary>
	/// <returns>当前秒</returns>
	int GetTimeNow_Sec();
	/// <summary>
	/// 获取当前毫秒
	/// </summary>
	/// <returns>当前毫秒</returns>
	int GetTimeNow_Msec();

	/// <summary>
	/// 获取格式化后的时间
	/// </summary>
	/// <returns>格式化后的时间</returns>
	std::string GetTimeNow_String();
	/// <summary>
	/// 获取格式化后的时间
	/// </summary>
	/// <returns>格式化后的时间</returns>
	std::wstring GetTimeNow_Wstring();
};

}