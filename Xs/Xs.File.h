#pragma once
#include "Xs.Main.h"
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <wrl/client.h>
#define Path std::filesystem::path
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")

#undef CreateFile
#undef CreateDirectory


namespace xs {

    namespace XFile {
        namespace Pick
        {
            //==========================================================
        // 文件过滤器
        //==========================================================
            using FileFilter = std::pair<std::wstring, std::wstring>;
            using FileFilters = std::vector<FileFilter>;
        

        /// <summary>
        /// 弹出选择文件的对话框
        /// </summary>
        /// <param name="title">对话框标题</param>
        /// <param name="filters">文件过滤器</param>
        /// <param name="initialDir">初始文件夹位置</param>
        /// <param name="multiSelect">是否允许多选</param>
        /// <returns>选择的文件路径</returns>
        std::vector<Path>
            PickFiles(
                const std::wstring& title = L"选择文件",
                const FileFilters& filters = {},
                const std::wstring& initialDir = {},
                bool multiSelect = true,HWND FHWNF = NULL
            );


        /// <summary>
        /// 弹出选择文件的对话框
        /// </summary>
        /// <param name="title">对话框标题</param>
        /// <param name="filters">文件过滤器</param>
        /// <param name="initialDir">初始文件夹位置</param>
        /// <param name="multiSelect">是否允许多选</param>
        /// <returns>选择的文件路径</returns>
        std::vector<Path>
            PickFiles(
                const std::string& title = "Select File",
                const std::vector<std::pair<std::string, std::string>>& filters = {},
                const std::string& initialDir = {},
                bool multiSelect = true, HWND FHWNF = NULL
            );

        /// <summary>
        /// 弹出选择文件夹的对话框
        /// </summary>
        /// <param name="title">对话框标题</param>
        /// <param name="initialDir">初始文件夹位置</param>
        /// <returns>选择的文件夹路径</returns>
        Path
            PickFolder(
                const std::wstring& title = L"选择文件夹",
                const std::wstring& initialDir = {}, HWND FHWNF = NULL
            );

        /// <summary>
        /// 弹出选择文件夹的对话框
        /// </summary>
        /// <param name="title">对话框标题</param>
        /// <param name="initialDir">初始文件夹位置</param>
        /// <returns>选择的文件夹路径</returns>
        Path
            PickFolder(
                const std::string& title = "Select Folder",
                const std::string& initialDir = {}, HWND FHWNF = NULL
            );
        };

        /// <summary>
        /// 判断文件或文件夹是否存在
        /// </summary>
        /// <param name="path">要判断的路径</param>
        /// <returns>如果存在返回true，否则返回false</returns>
        bool Exists(const Path& path);
        /// <summary>
       /// 判断文件是否存在
       /// </summary>
       /// <param name="path">要判断的路径</param>
       /// <returns>如果存在返回true，否则返回false</returns>
        bool IsFile(const Path& path);
        /// <summary>
       /// 判断文件夹是否存在
       /// </summary>
       /// <param name="path">要判断的路径</param>
       /// <returns>如果存在返回true，否则返回false</returns>
        bool IsDirectory(const Path& path);

        /// <summary>
        /// 创建目录
        /// </summary>
        /// <param name="path">要创建的目录的最子目录的路径</param>
        /// <returns>创建成功返回true</returns>
        bool CreateDirectory(const Path& path);

        /// <summary>
        /// 删除指定文件或文件夹
        /// </summary>
        /// <param name="path">要删除的文件或文件夹的路径</param>
        /// <returns>删除成功返回true</returns>
        bool Remove(const Path& path);

        /// <summary>
        /// 移动或重命名文件
        /// </summary>
        /// <param name="src">源文件路径</param>
        /// <param name="dst">新文件路径</param>
        /// <returns>重命名成功返回true</returns>
        bool Rename(const Path& src, const Path& dst);
        /// <summary>
        /// 复制文件到指定位置
        /// </summary>
        /// <param name="src">源文件路径</param>
        /// <param name="dst">目标文件路径</param>
        /// <param name="overwrite">是否覆盖</param>
        /// <returns>复制成功返回true</returns>
        bool Copy(const Path& src, const Path& dst, bool overwrite = true);

        /// <summary>
        /// 历遍指定文件夹里的文件
        /// </summary>
        /// <param name="dir">要历遍的文件夹</param>
        /// <returns>返回历遍的文件的路径</returns>
        std::vector<Path> ListFiles(const Path& dir);
        /// <summary>
        /// 历遍指定文件夹里的文件夹
        /// </summary>
        /// <param name="dir">要历遍的文件夹</param>
        /// <returns>返回历遍的文件夹的路径</returns>
        std::vector<Path> ListDirectories(const Path& dir);
        /// <summary>
        /// 历遍指定文件夹里的文件和文件夹
        /// </summary>
        /// <param name="dir">要历遍的文件夹</param>
        /// <returns>返回历遍的文件和文件夹的路径</returns>
        std::vector<Path> ListAll(const Path& dir);

        /// <summary>
        /// 获取文件的大小（字节）
        /// </summary>
        /// <param name="path">要获取的文件路径</param>
        /// <returns>文件的大小（字节）</returns>
        uint64_t Size(const Path& path);
        /// <summary>
        /// 获取路径下的文件名
        /// </summary>
        /// <param name="path">要获取的文件路径</param>
        /// <returns>文件名（带后缀）</returns>
        std::wstring DisplayName(const Path& path);
        /// <summary>
        /// 获取路径下的文件名
        /// </summary>
        /// <param name="path">要获取的文件路径</param>
        /// <returns>文件名（带后缀）</returns>
        std::string  DisplayNameA(const Path& path);
        /// <summary>
        /// 获取文件拓展名（后缀）
        /// </summary>
        /// <param name="path">要获取的文件路径</param>
        /// <returns>返回文件拓展名（后缀）</returns>
        std::wstring Extension(const Path& path);
        /// <summary>
       /// 获取文件拓展名（后缀）
       /// </summary>
       /// <param name="path">要获取的文件路径</param>
       /// <returns>返回文件拓展名（后缀）</returns>
        std::string  ExtensionA(const Path& path);

        namespace GetDir
        {

        /// <summary>
        /// 获取当前程序所在路径
        /// </summary>
        /// <returns>返回程序所在路径</returns>
        Path ModulePath();
        /// <summary>
        /// 获取系统软件路径
        /// </summary>
        /// <returns>返回系统软件路径</returns>
        Path ProgramDir();
        /// <summary>
        /// 获取系统缓存路径
        /// </summary>
        /// <returns>返回系统缓存路径</returns>
        Path TempPath();
        /// <summary>
        /// 获取系统桌面路径
        /// </summary>
        /// <returns>返回系统桌面路径</returns>
        Path Desktop();
        /// <summary>
        /// 获取系统文档路径
        /// </summary>
        /// <returns>返回系统文档路径</returns>
        Path Documents();
        /// <summary>
        /// 获取系统下载路径
        /// </summary>
        /// <returns>返回系统下载路径</returns>
        Path Downloads();
        /// <summary>
        /// 获取系统图片路径
        /// </summary>
        /// <returns>返回系统图片路径</returns>
        Path Pictures();
        /// <summary>
        /// 获取系统音乐路径
        /// </summary>
        /// <returns>返回系统音乐路径</returns>
        Path Music();
        /// <summary>
        /// 获取系统图片路径
        /// </summary>
        /// <returns>返回系统图片路径</returns>
        Path Videos();
        /// <summary>
        /// 获取系统AppDataLocal路径
        /// </summary>
        /// <returns>返回系统AppDataLocal路径</returns>
        Path AppDataLocal();
        /// <summary>
        /// 获取系统AppDataRoaming路径
        /// </summary>
        /// <returns>返回系统AppDataRoaming路径</returns>
        Path AppDataRoaming();
        /// <summary>
        /// 获取系统AppDataCommon路径
        /// </summary>
        /// <returns>返回系统AppDataCommin路径</returns>
        Path AppDataCommon();
        /// <summary>
        /// 获取系统System路径
        /// </summary>
        /// <returns>返回系统System路径</returns>
        Path SystemDir();
        /// <summary>
        /// 获取系统Windows路径
        /// </summary>
        /// <returns>返回系统Windows路径</returns>
        Path WindowsDir();
        };

        namespace DirCache
        {
            /// <summary>
           /// 保存路径
            /// </summary>
           /// <param name="key">标识</param>
           /// <param name="path">路径</param>
            void SetDirCache(const std::wstring& key, const Path& path);
            /// <summary>
            /// 查找保存的路径
            /// </summary>
            /// <param name="key">标识</param>
            /// <returns>返回找到的路径</returns>
            std::vector<Path> GetDirCache(const std::wstring& key);
            /// <summary>
            /// 删除保存的路径
            /// </summary>
            /// <param name="key">标识</param>
            void RemoveDirCache(const std::wstring& key);
            /// <summary>
            /// 清除保存目录
            /// </summary>
            void ClearDirCache();
        }
    }

} // namespace xs