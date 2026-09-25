#pragma once

#include "Xs/Xs.h"

class Write
{
public:
	static int TotalPage;
	static int Page;
	static int EraseSize;

	static void Show(RenWin& window);
	static void WriteT(RenWin& window);

	//新建板书
	static void MakeNewPage();

	//撤销
	static void UnDo();

	//擦除全部
	static void EraseAll();

	//启用/禁用 草稿层
	static void EnableTempLayer(bool enable);

	//上滑行/下滑行
	static void UpScrollPage();
	static void DownScrollPage();

	//下一页/加页
	static void NextPage();
	//上一页
	static void LastPage();

	//移动归位
	static void ResetMove();

	//跳转到指定页
	static void GoToPage(int page);

	//渲染指定页缩略图到纹理（不改变当前页）
	static void GetPageThumbnail(int pageIndex, RenderTexture& rt);
};


class ImageManager
{
public:
	static bool AddImage(std::wstring path,RenWin& window);
	static bool AddImageDirectly(std::wstring path);
};

class WriteFile
{
public:
	static void Save(std::wstring path,std::wstring ExtName = L"png");
	static void Load(std::wstring path);
};

class WriteCamera
{
public:
	static bool EnableWriteCamera,EnableAutoPhoto;

	static void Start();
	static void Stop();
	static void Draw(RenWin& window);
	static bool Manage();
	static bool IsRun();
	static void PhotoImage();

	static void Rote();

	static void ManageAutoPhoto();

	/// <summary>
	/// 获取指定相册页的照片图像。
	/// </summary>
	/// <param name="page">相册页索引；小于 1 时按 1 处理。</param>
	/// <returns>该页照片图像的引用。</returns>
	static xs::IMAGE& GetPhoto(int page);

	/// <summary>
	/// 获取指定相册页的照片旋转度
	/// </summary>
	/// <param name="page">页数</param>
	/// <returns></returns>
	static int GetRote(int page);
};