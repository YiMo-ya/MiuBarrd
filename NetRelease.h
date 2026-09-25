#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <cstdint>

// 保证 winhttp.lib 被链接（也可在项目属性里手动添加）
#pragma comment(lib, "winhttp.lib")

/// <summary>
/// Gitee Release 下载任务的状态快照。
/// 该结构体由后台下载线程写入，由 GetProcess 读取，字段均为原子类型以保证线程安全。
/// </summary>
struct ReleaseProcess
{
	/// <summary>下载是否正在进行中（true=运行中）</summary>
	std::atomic<bool>      Running{ false };
	/// <summary>下载是否已成功完成</summary>
	std::atomic<bool>      Finished{ false };
	/// <summary>是否发生错误（详见 ErrorMsg）</summary>
	std::atomic<bool>      Failed{ false };
	/// <summary>已接收的字节数</summary>
	std::atomic<uint64_t>  Downloaded{ 0 };
	/// <summary>文件总字节数（服务器未返回 Content-Length 时为 0）</summary>
	std::atomic<uint64_t>  Total{ 0 };
	/// <summary>进度百分比，取值范围 0.0 ~ 100.0（Total 为 0 时恒为 0）</summary>
	std::atomic<double>    Percent{ 0.0 };
	/// <summary>最后一次错误描述（UTF-8）</summary>
	std::string            ErrorMsg;
	/// <summary>最终保存的本地文件完整路径（UTF-8）</summary>
	std::string            SavedPath;
};

/// <summary>
/// 向 Gitee 指定仓库的最新 Release 发送下载请求，并把资产文件保存到
/// 「当前程序所在目录\Update\」下。
/// 该函数在后台线程执行，立即返回；进度通过 GetProcess 查询。
/// </summary>
/// <param name="owner">仓库拥有者（用户名或组织名），例如 "myuser"</param>
/// <param name="repo">仓库名，例如 "MiuBarrd"</param>
/// <param name="assetName">
/// 要下载的资产文件名；若为空字符串，则下载该 Release 的第一个资产。
/// </param>
/// <returns>true 表示任务已成功启动；false 表示已有任务在运行或参数非法</returns>
bool DownloadRelease(const std::string& owner,
	const std::string& repo,
	const std::string& assetName = "");

/// <summary>
/// 通过已知的直链发起下载（不经过 Release API 解析）。
/// 适用于你已经自己拿到 download_url 的场景，同样在后台线程执行。
/// </summary>
/// <param name="url">完整下载地址（http/https）</param>
/// <param name="saveName">
/// 保存文件名；为空时从 URL 末段推断，推断失败则用 "download.bin"。
/// </param>
/// <returns>true 表示任务已成功启动；false 表示已有任务在运行或参数非法</returns>
bool DownloadUrl(const std::string& url, const std::string& saveName = "");

/// <summary>
/// 实时返回当前下载进度快照（线程安全，可在渲染循环/主线程中高频调用）。
/// </summary>
/// <param name="out">
/// 输出参数，拷贝当前进度；请先调用方自行判断 Finished / Failed 再读取结果。
/// </param>
/// <returns>true 表示查询成功（始终返回 true，除非 out 指针为空）</returns>
bool GetProcess(ReleaseProcess& out);

/// <summary>
/// 同步解析指定仓库最新 Release 中目标资产的下载直链。
/// 该函数为同步阻塞调用，仅用于简单场景。
/// </summary>
/// <param name="owner">仓库拥有者</param>
/// <param name="repo">仓库名</param>
/// <param name="assetName">资产文件名，空则取第一个</param>
/// <returns>资产下载直链；失败返回空字符串</returns>
std::string ResolveReleaseAssetUrl(const std::string& owner,
	const std::string& repo,
	const std::string& assetName = "");

/// <summary>
/// 重置进度状态，使 DownloadRelease 可再次调用（在任务结束后调用）。
/// </summary>
void ResetProcess();

/// <summary>
/// 向 Gitee 指定仓库的最新 Release 发送请求，解析并返回其版本号（tag_name）。
/// 该函数为同步阻塞调用，适用于查询远端最新版本用于比对。
/// </summary>
/// <param name="owner">仓库拥有者（用户名或组织名）</param>
/// <param name="repo">仓库名</param>
/// <returns>解析到的版本号（宽字符串）；失败返回空字符串</returns>
std::wstring GetGiteeReleaseVersion(const std::string& owner,
	const std::string& repo);
