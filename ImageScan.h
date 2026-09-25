#pragma once
#include "Xs/Xs.Main.h"

/// 拍照文档矫正：把「深色背景上的浅色纸张」拉正、去光照不均、背景抠透明。
namespace ImgScan
{
    /// 矫正参数（默认值适用于“白纸 + 近黑背景”的拍照场景）
    struct Options
    {
        bool  enableWarp = true;   ///< 是否做透视/倾斜矫正；关掉则只做色调矫正
        bool  enableMask = true;   ///< 是否把纸张以外的背景抠成透明
        float blackPoint = 0.35f;  ///< 归一化黑场：低于此亮度压成纯黑
        float whitePoint = 0.88f;  ///< 归一化白场：高于此亮度提成纯白
        float gamma = 1.0f;   ///< 中间调伽马；>1 更亮，<1 更暗
        float featherPx = 0.0f;   ///< 成品边缘羽化宽度（像素）；0=不羽化，避免透明黑背景在纸边形成黑晕
        float colorKeep = 1.0f;   ///< 0=纯灰度文档，1=完全保留原色（默认保留原色，避免扫描后颜色变黑白）
        float maskLow = 0.25f;  ///< 背景判定下限（相对纸张亮度）
        float maskHigh = 0.55f;  ///< 背景判定上限
    };

    /// 就地矫正纹理：透视拉正 + 光照均一化 + 黑白场拉伸 + 背景透明。
    /// @param tex 待处理纹理；处理结束后会被 resize 成矫正后的尺寸并写入结果。
    /// @param opt 矫正参数。
    /// @return 成功返回 true；着色器不可用或单应矩阵退化时返回 false（此时 tex 保持不变）。
    /// @note 内部会创建 RenderTexture，必须在持有 OpenGL 上下文的线程调用。
    bool Correct(Texture& tex, const Options& opt = {});
}
