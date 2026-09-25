#pragma once
#include "Xs.Main.h"

namespace xs
{
	namespace XDebug
	{
		/// <summary>
		/// 写入日志
		/// </summary>
		/// <param name="text">要写入的日志内容</param>
		/// <param name="LogFilePath">日志文件路径</param>
		void Log(const String& text, const String& LogFilePath = L"XsDebug.log");
		/// <summary>
		/// 使用MessageBox显示调试信息
		/// </summary>
		/// <param name="text">要显示的调试信息</param>
		void DebugMsg(const String& text);

		/// <summary>
		/// 将调试信息送至缓冲区。
		/// </summary>
		/// <param name="text">调试信息</param>
		/// <param name="text">等级</param>
		void DebugPut(const String& text,int level = 0);

		/// <summary>
		/// 获取缓冲区的调试信息
		/// </summary>
		/// <returns>缓冲区的调试信息</returns>
		std::vector<std::pair<String,int>> GetDebug();

		/// <summary>
		/// 清除调试信息缓冲区
		/// </summary>
		void ClearDebug();
	};
}