#include "NetRelease.h"

#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <vector>
#include <mutex>
#include <algorithm>

namespace
{
	// 全局唯一进度对象与互斥，保证同一时刻仅有一个下载任务
	ReleaseProcess g_process;
	std::mutex     g_procMutex;
	std::thread    g_worker;

	// 简易 UTF-8 <-> UTF-16 转换（避免引入 Xs 依赖，便于独立使用）
	std::wstring Utf8ToWide(const std::string& s)
	{
		if (s.empty()) return std::wstring();
		int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
		std::wstring out(len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
		return out;
	}

	std::string WideToUtf8(const std::wstring& s)
	{
		if (s.empty()) return std::string();
		int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(),
			nullptr, 0, nullptr, nullptr);
		std::string out(len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(),
			&out[0], len, nullptr, nullptr);
		return out;
	}

	// 获取当前程序（exe）所在目录，不含结尾反斜杠
	std::wstring GetExeDirectory()
	{
		wchar_t buf[MAX_PATH] = { 0 };
		GetModuleFileNameW(nullptr, buf, MAX_PATH);
		std::wstring path(buf);
		size_t pos = path.find_last_of(L"\\/");
		return (pos == std::wstring::npos) ? path : path.substr(0, pos);
	}

	// 保证目录存在；返回是否可用
	bool EnsureDirectory(const std::wstring& dir)
	{
		DWORD attr = GetFileAttributesW(dir.c_str());
		if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
			return true;
		return CreateDirectoryW(dir.c_str(), nullptr) != FALSE;
	}

	// 从 URL 末段推断文件名
	std::string GuessFileName(const std::string& url)
	{
		size_t q = url.find('?');
		std::string clean = (q == std::string::npos) ? url : url.substr(0, q);
		size_t slash = clean.find_last_of('/');
		if (slash == std::string::npos || slash + 1 >= clean.size())
			return "download.bin";
		return clean.substr(slash + 1);
	}

	// 打开 WinHTTP 会话与连接，返回会话句柄（连接句柄经 outConnect 返回）；失败返回 nullptr
	HINTERNET OpenSession(const std::wstring& host, INTERNET_PORT port,
		HINTERNET& outConnect)
	{
		HINTERNET session = WinHttpOpen(L"MiuBarrd/1.0",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session) return nullptr;

		// 允许跟随 Gitee 常见的重定向（Release 资产会 302 到对象存储）
		DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY,
			&redirect, sizeof(redirect));

		outConnect = WinHttpConnect(session, host.c_str(), port, 0);
		if (!outConnect)
		{
			WinHttpCloseHandle(session);
			return nullptr;
		}
		return session;
	}

	// 发送 GET 请求并返回请求句柄（调用方负责关闭）
	HINTERNET SendGet(HINTERNET connect, const std::wstring& path, bool https)
	{
		DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(),
			nullptr, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!request) return nullptr;

		if (!WinHttpSendRequest(request,
			WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
		{
			WinHttpCloseHandle(request);
			return nullptr;
		}
		if (!WinHttpReceiveResponse(request, nullptr))
		{
			WinHttpCloseHandle(request);
			return nullptr;
		}
		return request;
	}

	// 读取全部响应体到 string（用于解析 JSON）
	std::string ReadAll(HINTERNET request)
	{
		std::string result;
		DWORD avail = 0;
		do
		{
			avail = 0;
			if (!WinHttpQueryDataAvailable(request, &avail) || avail == 0)
				break;
			std::vector<char> buf(avail);
			DWORD read = 0;
			if (!WinHttpReadData(request, buf.data(), avail, &read) || read == 0)
				break;
			result.append(buf.data(), read);
		} while (avail > 0);
		return result;
	}

	// 解析 URL 为 (host, path, port, https)
	bool ParseUrl(const std::string& url, std::wstring& host, std::wstring& path,
		INTERNET_PORT& port, bool& https)
	{
		std::wstring wurl = Utf8ToWide(url);
		URL_COMPONENTS uc;
		ZeroMemory(&uc, sizeof(uc));
		uc.dwStructSize = sizeof(uc);

		wchar_t hostBuf[256] = { 0 };
		wchar_t pathBuf[2048] = { 0 };
		uc.lpszHostName = hostBuf; uc.dwHostNameLength = _countof(hostBuf);
		uc.lpszUrlPath = pathBuf;  uc.dwUrlPathLength = _countof(pathBuf);

		if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc))
			return false;

		host = hostBuf;
		// 包含查询串，否则 Gitee 直链会丢失参数
		path = pathBuf;
		port = uc.nPort;
		https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
		return true;
	}

	// 在 JSON 中查找第一个键为 key 的字符串值（极简解析，够用即可）
	bool FindJsonString(const std::string& json, const std::string& key, std::string& value)
	{
		std::string needle = "\"" + key + "\"";
		size_t p = json.find(needle);
		if (p == std::string::npos) return false;
		size_t colon = json.find(':', p + needle.size());
		if (colon == std::string::npos) return false;
		size_t q1 = json.find('"', colon);
		if (q1 == std::string::npos) return false;
		size_t q2 = json.find('"', q1 + 1);
		if (q2 == std::string::npos) return false;
		value = json.substr(q1 + 1, q2 - q1 - 1);
		return true;
	}

	// 请求最新 Release 的 JSON 正文，供上层复用解析（同步阻塞）
	std::string FetchLatestReleaseJson(const std::string& owner, const std::string& repo)
	{
		std::string url = "https://gitee.com/api/v5/repos/" + owner + "/" + repo +
			"/releases/latest";

		std::wstring host, path;
		INTERNET_PORT port = 0;
		bool https = false;
		if (!ParseUrl(url, host, path, port, https))
			return "";

		HINTERNET connect = nullptr;
		HINTERNET session = OpenSession(host, port, connect);
		if (!session) return "";

		std::string body;
		HINTERNET request = SendGet(connect, path, https);
		if (request)
		{
			body = ReadAll(request);
			WinHttpCloseHandle(request);
		}
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return body;
	}

	// 解析最新 Release 的资产列表，提取目标资产的 download_url。
	// 极简实现：扫描所有 "name" 与紧随其后的 "download_url" 配对。
	std::string ResolveAssetUrl(const std::string& owner, const std::string& repo,
		const std::string& assetName)
	{
		std::string url = "https://gitee.com/api/v5/repos/" + owner + "/" + repo +
			"/releases/latest";

		std::wstring host, path;
		INTERNET_PORT port = 0;
		bool https = false;
		if (!ParseUrl(url, host, path, port, https))
			return "";

		HINTERNET connect = nullptr;
		HINTERNET session = OpenSession(host, port, connect);
		if (!session) return "";

		std::string body;
		HINTERNET request = SendGet(connect, path, https);
		if (request)
		{
			body = ReadAll(request);
			WinHttpCloseHandle(request);
		}
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);

		if (body.empty()) return "";

		// 逐个资产块扫描：先找到 name 再找紧随其后的 download_url
		std::string result;
		size_t cursor = 0;
		std::string firstUrl;

		while (true)
		{
			size_t namePos = body.find("\"name\"", cursor);
			if (namePos == std::string::npos) break;

			std::string nameVal;
			if (!FindJsonString(body.substr(namePos), "name", nameVal))
			{
				cursor = namePos + 6;
				continue;
			}

			// name 与 download_url 通常在同一资产对象中，download_url 在其后
			std::string tail = body.substr(namePos);
			std::string urlVal;
			if (FindJsonString(tail, "download_url", urlVal))
			{
				if (firstUrl.empty())
					firstUrl = urlVal;
				if (assetName.empty() || nameVal == assetName)
				{
					result = urlVal;
					break;
				}
			}

			cursor = namePos + 6;
		}

		if (result.empty() && assetName.empty())
			result = firstUrl;

		return result;
	}

	// 真正的下载逻辑（在后台线程运行）
	void DownloadWorker(std::string url, std::string saveName)
	{
		{
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Running = true;
			g_process.Finished = false;
			g_process.Failed = false;
			g_process.Downloaded = 0;
			g_process.Total = 0;
			g_process.Percent = 0.0;
			g_process.ErrorMsg.clear();
			g_process.SavedPath.clear();
		}

		std::wstring host, path;
		INTERNET_PORT port = 0;
		bool https = false;
		if (!ParseUrl(url, host, path, port, https))
		{
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Failed = true;
			g_process.Running = false;
			g_process.ErrorMsg = "URL 解析失败";
			return;
		}

		HINTERNET connect = nullptr;
		HINTERNET session = OpenSession(host, port, connect);
		if (!session)
		{
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Failed = true;
			g_process.Running = false;
			g_process.ErrorMsg = "无法建立连接";
			return;
		}

		HINTERNET request = SendGet(connect, path, https);
		if (!request)
		{
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Failed = true;
			g_process.Running = false;
			g_process.ErrorMsg = "请求发送失败";
			return;
		}

		// 读取 HTTP 状态码，非 200 直接失败
		DWORD status = 0, len = sizeof(status);
		WinHttpQueryHeaders(request,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
		if (status != 200)
		{
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Failed = true;
			g_process.Running = false;
			g_process.ErrorMsg = "服务器返回状态码 " + std::to_string(status);
			return;
		}

		uint64_t total = 0;
		wchar_t clen[64] = { 0 };
		DWORD clenSize = sizeof(clen);
		if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
			WINHTTP_HEADER_NAME_BY_INDEX, clen, &clenSize, WINHTTP_NO_HEADER_INDEX))
		{
			total = _wcstoui64(clen, nullptr, 10);
		}
		{
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Total = total;
		}

		// 确定保存路径：exe 目录\Update\文件名
		std::wstring dir = GetExeDirectory() + L"\\Update";
		if (!EnsureDirectory(dir))
		{
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Failed = true;
			g_process.Running = false;
			g_process.ErrorMsg = "无法创建 Update 目录";
			return;
		}

		if (saveName.empty()) saveName = GuessFileName(url);
		std::wstring fullPath = dir + L"\\" + Utf8ToWide(saveName);

		{
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.SavedPath = WideToUtf8(fullPath);
		}

		std::ofstream ofs(fullPath, std::ios::binary | std::ios::trunc);
		if (!ofs.is_open())
		{
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Failed = true;
			g_process.Running = false;
			g_process.ErrorMsg = "无法创建目标文件";
			return;
		}

		// 分块读取并持续更新进度
		uint64_t received = 0;
		DWORD avail = 0;
		bool ok = true;
		do
		{
			avail = 0;
			if (!WinHttpQueryDataAvailable(request, &avail))
			{
				ok = false;
				break;
			}
			if (avail == 0) break;

			// 限制单次缓冲大小，避免大文件一次性占用过多内存
			DWORD chunk = (avail > 1024 * 1024) ? 1024 * 1024 : avail;
			std::vector<char> buf(chunk);
			DWORD read = 0;
			if (!WinHttpReadData(request, buf.data(), chunk, &read) || read == 0)
			{
				ok = false;
				break;
			}

			ofs.write(buf.data(), read);
			received += read;

			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Downloaded = received;
			if (g_process.Total > 0)
				g_process.Percent = 100.0 * double(received) / double(g_process.Total.load());
		} while (avail > 0);

		ofs.close();

		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);

		{
			std::lock_guard<std::mutex> lock(g_procMutex);
			g_process.Running = false;
			if (ok && (!g_process.Total || received == g_process.Total))
			{
				g_process.Finished = true;
				g_process.Percent = 100.0;
			}
			else
			{
				g_process.Failed = true;
				g_process.ErrorMsg = "下载中断或数据不完整";
			}
		}
	}

	// 启动后台线程；若已有任务运行则拒绝
	bool StartWorker(std::string url, std::string saveName)
	{
		if (url.empty()) return false;

		// 回收上一个已结束的线程
		if (g_worker.joinable())
		{
			if (g_process.Running.load())
				return false; // 仍在运行
			g_worker.join();
		}

		ResetProcess();
		g_worker = std::thread(DownloadWorker, std::move(url), std::move(saveName));
		return true;
	}
}

bool DownloadRelease(const std::string& owner, const std::string& repo,
	const std::string& assetName)
{
	if (owner.empty() || repo.empty()) return false;
	std::string url = ResolveAssetUrl(owner, repo, assetName);
	if (url.empty()) return false;
	return StartWorker(url, assetName);
}

bool DownloadUrl(const std::string& url, const std::string& saveName)
{
	return StartWorker(url, saveName);
}

bool GetProcess(ReleaseProcess& out)
{
	std::lock_guard<std::mutex> lock(g_procMutex);
	out.Running = g_process.Running.load();
	out.Finished = g_process.Finished.load();
	out.Failed = g_process.Failed.load();
	out.Downloaded = g_process.Downloaded.load();
	out.Total = g_process.Total.load();
	out.Percent = g_process.Percent.load();
	out.ErrorMsg = g_process.ErrorMsg;
	out.SavedPath = g_process.SavedPath;
	return true;
}

std::string ResolveReleaseAssetUrl(const std::string& owner,
	const std::string& repo, const std::string& assetName)
{
	return ResolveAssetUrl(owner, repo, assetName);
}

std::wstring GetGiteeReleaseVersion(const std::string& owner,
	const std::string& repo)
{
	if (owner.empty() || repo.empty()) return std::wstring();

	// 1. 请求最新 Release 的 JSON 正文
	std::string body = FetchLatestReleaseJson(owner, repo);
	if (body.empty()) return std::wstring();

	// 2. Gitee Release 的版本号字段为 tag_name，形如 "v1.2.3"
	std::string tag;
	if (!FindJsonString(body, "tag_name", tag))
		return std::wstring();

	// 3. 去掉可能存在的 "v"/"V" 前缀，仅保留纯版本号
	if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V'))
		tag.erase(0, 1);

	// 4. 转为宽字符串返回（UTF-8 -> UTF-16）
	return Utf8ToWide(tag);
}

void ResetProcess()
{
	std::lock_guard<std::mutex> lock(g_procMutex);
	g_process.Running = false;
	g_process.Finished = false;
	g_process.Failed = false;
	g_process.Downloaded = 0;
	g_process.Total = 0;
	g_process.Percent = 0.0;
	g_process.ErrorMsg.clear();
	g_process.SavedPath.clear();
}
