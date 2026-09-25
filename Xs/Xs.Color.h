#pragma once
#include "Xs.Main.h"

namespace xs
{
    namespace XColor
    {
        /// <summary>
        /// 灰度化颜色
        /// </summary>
        /// <param name="color">目标颜色</param>
        /// <param name="percent">灰度值 [0 - 100]</param>
        /// <returns>修改后的颜色</returns>
        Color GrayColor(const Color& color, float percent);
        /// <summary>
        /// 暗化颜色
        /// </summary>
        /// <param name="color">目标颜色</param>
        /// <param name="percent">暗度值 [0 - 100]</param>
        /// <returns>修改后的颜色</returns>
        Color DarkColor(const Color& color, float percent);
        /// <summary>
       /// 亮化颜色
       /// </summary>
       /// <param name="color">目标颜色</param>
       /// <param name="percent">亮度值 [0 - 100]</param>
       /// <returns>修改后的颜色</returns>
        Color LightColor(const Color& color, float percent);
        /// <summary>
        /// 透明化颜色
        /// </summary>
        /// <param name="color">目标颜色</param>
        /// <param name="percent">透明值 [0 - 255]</param>
        /// <returns>修改后的颜色</returns>
        Color AlphaColor(const Color& color, uint8_t alpha);
        /// <summary>
        /// 判断是否为深色
        /// </summary>
        /// <param name="color">要判断的颜色</param>
        /// <returns>如果是，返回true，否则返回false</returns>
        bool IsDarkColor(const Color& color);
        /// <summary>
        /// 判断颜色是否相似或者相同
        /// </summary>
        /// <param name="colorA">目标颜色</param>
        /// <param name="colorB">目标对比颜色</param>
        /// <param name="UseAlpha">是否对比Alpha，默认不对比</param>
        /// <param name="isExact">是否精确对比，而不是相似对比</param>
        /// <param name="VagueValue">相似差度</param>
        /// <returns>如果相似或相同，返回true，否则返回false</returns>
        bool IsColorSame(const Color& colorA, const Color& colorB, bool UseAlpha = false, bool isExact = false, int VagueValue = 10);
        /// <summary>
        /// 调整颜色亮度
        /// </summary>
        /// <param name="color">目标颜色</param>
        /// <param name="brightness">亮度阈值</param>
        /// <returns>调整后的颜色</returns>
        Color AdjustColorBright(const Color& color, int brightness);
    };
}
