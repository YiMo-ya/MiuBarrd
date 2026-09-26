#pragma once

#include "Xs/Xs.h"
#include "User.h"

// 统一命名空间，供本头文件内的 MindMap::Node 使用 EV / vector / Color
NOXS; NOSTD;

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

class MindMap
{
public:
	/// <summary>
	/// 思维导图节点，一个节点代表一个手写内容块及其相对父节点的结构信息。
	/// </summary>
	struct Node
	{
		int id = -1;                 // 节点唯一编号，-1 表示无效
		int parent = -1;             // 父节点编号，-1 表示根节点

		int depth = 0;               // 层级深度：根为 0，子节点依次 +1

		int x = 0, y = 0;            // 节点内容左上角坐标（屏幕/层坐标）
		int lineW = 0;               // 底线当前长度（动态拓展后的长度）
		int lineH = 0;               // 内容区高度（用于父子的空隙计算）

		EV lineEase;                 // 底线长度的缓动（动态拓展动画）

		Color color = User::MainColor; // 该节点底线/圆形按钮的颜色

		vector<int> children;        // 子节点编号列表（按添加顺序）
	};

	/// <summary>
	/// 添加思维导图：未激活时创建根节点并激活；已存在时不再重复创建，
	/// 而是弹出提示告知用户「当前页面只能存在一个思维导图」。
	/// 注意：节点级「新增子项」是点击界面上的 + 按钮，
	/// 其内部同样走本方法，因此本方法不再承担「追加并列子节点」的职责。
	/// </summary>
	static void Add();

	/// <summary>
	/// 在当前选中节点（无有效选中时为根节点）下追加一个子节点，
	/// 供界面上的 + 按钮调用；不涉及单实例判定。
	/// </summary>
	/// <returns>新节点的编号；当前无任何节点时返回 -1。</returns>
	static int AddChild();

	/// <summary>
	/// 删除当前选中节点（根节点不可删除，删除会连带其子树）。
	/// </summary>
	static void Del();

	/// <summary>
	/// 绘制整棵思维导图到目标渲染目标。
	/// </summary>
	/// <param name="dest">目标渲染目标（窗口或思维导图层）。</param>
	static void Draw(RenderTarget& dest);

	/// <summary>
	/// 每帧更新：处理节点底线动态拓展、+ 按钮点击检测、节点选中。
	/// </summary>
	static void Update();

	/// <summary>
	/// 查询本帧触控是否落在思维导图的 + 按钮上。
	/// </summary>
	/// <returns>落在按钮上返回 true，否则返回 false。</returns>
	static bool IsBtnHit();

	/// <summary>
	/// 取出并清除「请求调色」的节点编号。
	/// 外部（工具栏/调色窗口）每帧调用一次：返回非 -1 表示用户点击了某节点的调色按钮，
	/// 调用后内部立即复位为 -1，避免重复弹出调色窗口。
	/// </summary>
	/// <returns>请求调色的节点编号；无请求时返回 -1。</returns>
	static int TakeColorRequest();

	/// <summary>
	/// 重置整个思维导图（清空所有节点并重新生成根节点），并激活显示。
	/// </summary>
	static void Reset();

	/// <summary>
	/// 清屏思维导图：移除所有节点并回到未激活状态，
	/// 用于与画布"擦除全部"保持一致的清屏语义。
	/// </summary>
	static void Clear();
};