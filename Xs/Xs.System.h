#pragma once
#include "Xs.Main.h"
#include <shobjidl.h>
#include <atomic>
#undef ERROR
//=======================================
/*
* 系统函数
*/

namespace xs
{
	//系统
	namespace XSystem
	{
		//任务栏
		namespace Taskbar {
		

			/// <summary>
			/// 显示或隐藏任务栏上的图标
			/// </summary>
			/// <param name="hwnd">目标句柄</param>
			/// <param name="show">是否显示</param>
			void ShowTaskbarButton(HWND hwnd, bool show = true);
			/// <summary>
			/// 显示或隐藏任务栏上的图标
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="show">是否显示</param>
			void ShowTaskbarButton(RenWin& window, bool show = true);

			// 进度条状态
			enum ProgressState {
				NOPPROGRESS = TBPF_NOPROGRESS,
				INDETERMINATE = TBPF_INDETERMINATE,
				NORMAL = TBPF_NORMAL,
				ERROR = TBPF_ERROR,
				PAUSED = TBPF_PAUSED
			};

			/// <summary>
			/// 设置任务栏上的图标的状态
			/// </summary>
			/// <param name="hwnd">目标句柄</param>
			/// <param name="state">状态</param>
			void SetProgressState(HWND hwnd, ProgressState state);
			/// <summary>
			/// 设置任务栏上的图标的状态
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="state">状态</param>
			void SetProgressState(RenWin& window, ProgressState state);

			/// <summary>
			/// 设置任务栏上的图标的加载进度
			/// </summary>
			/// <param name="hwnd">目标句柄</param>
			/// <param name="current">当前值</param>
			/// <param name="total">总共值</param>
			void SetProgressValue(HWND hwnd, ULONGLONG current, ULONGLONG total);
			/// <summary>
			/// 设置任务栏上的图标的加载进度
			/// </summary>
			/// <param name="window">目标窗口</param>
			/// <param name="current">当前值</param>
			/// <param name="total">总共值</param>
			void SetProgressValue(RenWin& window, ULONGLONG current, ULONGLONG total);

			/// <summary>
			/// 设置任务栏是否可见
			/// </summary>
			/// <param name="IsVisible">是否显示</param>
			void SetTaskBarVisible(bool IsVisible = true);
		};
		//托盘
		namespace Tray {
		
			/// <summary>
			/// 绑定窗口到托盘上用于接受消息
			/// </summary>
			/// <param name="hwnd">目标窗口</param>
			/// <returns>成功返回true</returns>
			bool BindWindow(HWND hwnd);

			/// <summary>
			/// 向托盘中添加图标
			/// </summary>
			/// <param name="tooltip">托盘提示</param>
			/// <param name="resId">资源Id，为0时加载程序图标</param>
			/// <returns>成功返回true</returns>
			bool AddIcon(std::wstring tooltip = nullptr,UINT resId = 0);
			/// <summary>
			/// 向托盘中添加图标
			/// </summary>
			/// <param name="tooltip">托盘提示</param>
			/// <param name="resId">资源Id，为0时加载程序图标</param>
			/// <returns>成功返回true</returns>
			bool AddIcon(std::string tooltip = nullptr, UINT resId = 0);

			/// <summary>
			/// 更新图标
			/// </summary>
			/// <param name="resId">资源Id，为0时加载程序图标</param>
			/// <returns>成功返回true</returns>
			bool SetIcon(UINT resId = 0);

			/// <summary>
			/// 更新托盘提示
			/// </summary>
			/// <param name="tooltip">托盘提示</param>
			/// <returns>成功返回true</returns>
			bool SetTooltip(std::wstring tooltip);
			/// <summary>
			/// 更新托盘提示
			/// </summary>
			/// <param name="tooltip">托盘提示</param>
			/// <returns>成功返回true</returns>
			bool SetTooltip(std::string tooltip);

			//气泡提示
			enum InfoType {
				BALLOON_NONE = NIIF_NONE,//默认
				BALLOON_INFO = NIIF_INFO,//通知
				BALLOON_WARNING = NIIF_WARNING,//警告
				BALLOON_ERROR = NIIF_ERROR//错误
			};
			/// <summary>
			/// 显示气泡通知
			/// </summary>
			/// <param name="title">标题</param>
			/// <param name="text">文本</param>
			/// <param name="type">通知类型</param>
			/// <param name="timeoutMs">超时</param>
			/// <returns>成功返回true</returns>
			bool ShowBalloon(
				LPCWSTR title,
				LPCWSTR text,
				InfoType type = InfoType::BALLOON_INFO,
				DWORD timeoutMs = 5000
			);

			/// <summary>
			/// 删除托盘图标
			/// </summary>
			/// <returns>成功返回true</returns>
			bool RemoveIcon();
		};
		//信息检索
		namespace Info
		{
		
			Vector2i GetScreenSize();
			Vector2i GetWorkRectSize();
			Vector2i GetTaskbarSize();
		};
	};
};

