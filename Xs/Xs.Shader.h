#pragma once
#include "Xs.Main.h"

namespace xs
{

namespace XShader
{
    /// <summary>
    /// 为纹理应用着色器效果
    /// </summary>
    /// <param name="texture">纹理</param>
    /// <param name="shader">着色器</param>
    /// <param name="bind">绑定</param>
    void ApplyShader(
        Texture& texture,
        sf::Shader& shader,
        std::function<void(sf::Shader&)> bind = nullptr);

    /// <summary>
    /// 加载顶点+片段的着色器
    /// </summary>
    /// <param name="key">着色器引索</param>
    /// <param name="vertPath">着色器顶点路径</param>
    /// <param name="fragPath">着色器片段路径</param>
    /// <param name="注意">请使用GetShader函数查找并使用着色器</param>
    /// <returns>如果加载成功返回true</returns>
    bool load(const std::string& key,
        const std::filesystem::path& vertPath,
        const std::filesystem::path& fragPath);

    /// <summary>
    /// 只加载片段着色器
    /// </summary>
    /// <param name="key">着色器引索</param>
    /// <param name="fragPath">着色器片段路径</param>
    /// <returns>如果加载成功返回true</returns>
    bool loadFragment(const std::string& key,
        const std::filesystem::path& fragPath);

    /// <summary>
    /// 只加载顶点着色器
    /// </summary>
    /// <param name="key">着色器引索</param>
    /// <param name="fragPath">着色器顶点路径</param>
    /// <returns>如果加载成功返回true</returns>
    bool loadVertex(const std::string& key,
        const std::filesystem::path& vertPath);

    /// <summary>
    /// 加载顶点+片段的着色器
    /// </summary>
    /// <param name="key">着色器引索</param>
    /// <param name="vertSource">着色器顶点</param>
    /// <param name="fragSource">着色器片段</param>
    /// <param name="注意">请使用GetShader函数查找并使用着色器</param>
    /// <returns>如果加载成功返回true</returns>
    bool load(const std::string& key,
        std::string_view vertSource,
        std::string_view fragSource);

    /// <summary>
    /// 只加载片段着色器
    /// </summary>
    /// <param name="key">着色器引索</param>
    /// <param name="fragSource">着色器片段</param>
    /// <returns>如果加载成功返回true</returns>
    bool loadFragment(const std::string& key,
        std::string_view fragSource);

    /// <summary>
    /// 只加载顶点着色器
    /// </summary>
    /// <param name="key">着色器引索</param>
    /// <param name="vertSource">着色器顶点</param>
    /// <returns>如果加载成功返回true</returns>
    bool loadVertex(const std::string& key,
        std::string_view vertSource);

    /// <summary>
    /// 获取加载的着色器
    /// </summary>
    /// <param name="key">着色器引索</param>
    /// <returns>返回着色器指针，没有返回nullptr</returns>
    sf::Shader* GetShader(const std::string& key);
    /// <summary>
    /// 检查是否存在指定的着色器
    /// </summary>
    /// <param name="key">着色器引索</param>
    /// <returns>如果存在返回true</returns>
    bool HasShader(const std::string& key);

    /// <summary>
    /// 删除加载的着色器
    /// </summary>
    /// <param name="key">着色器引索</param>
    void RemoveShader(const std::string& key);

    /// <summary>
    /// 清除所有加载的着色器
    /// </summary>
    void ClearShader();
};

}