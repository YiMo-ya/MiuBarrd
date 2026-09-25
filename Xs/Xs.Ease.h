#pragma once
#include "Xs.Main.h"

namespace xs
{
    //动画变量
    struct easevalue
    {
        float start = 0, end = 0;

        float value = 0;

        int frame = 999;
        double totalframe = 30;

        /// <summary>
        /// 设置动画
        /// </summary>
        /// <param name="EndValue">目标值</param>
        /// <param name="TotlaFrame">总帧率，为小于0的值时不干涉</param>
        void SetAnimation(float EndValue,int TotlaFrame = 0)
        {
            frame = 0;
            start = value;
            end = EndValue;
            if (TotlaFrame > 0)
            {
                totalframe = (double)TotlaFrame;
            }
        }
        /// <summary>
        /// 更新动画起始值
        /// </summary>
        /// <param name="Value">动画起始值</param>
        void SetAnimationStartValue(int Value)
        {
            value = start = Value;
        }
        /// <summary>
        /// 判断当前值是否正在动画
        /// </summary>
        /// <returns>是返回true，否则返回false</returns>
        bool IsAnimation() const
        {
            return frame <= totalframe;
        }
        /// <summary>
		/// 更新动画值
        /// </summary>
        /// <param name="func">目标Ease函数</param>
        /// <param name="exp">强度</param>
        /// <param name="TotalFrame">总帧数</param>
        void UpdateAnimation(std::function<double(double t, double exp)> func,double exp)
        {
            if(frame <= totalframe)
            {
                double t = (double)frame / totalframe;
                value = start + (end - start) * func(t, exp);
                frame++;
            }
        }
        /// <summary>
        /// 更新动画值
        /// </summary>
        /// <param name="func">目标Ease函数</param>
        /// /// <param name="TotalFrame">总帧数</param>
        void UpdateAnimation(std::function<double(double t)> func)
        {
            if (frame <= totalframe)
            {
                double t = (double)frame / totalframe;
                value = start + (end - start) * func(t);
                frame++;
            }
        }
        /// <summary>
        /// 更新动画值
        /// </summary>
        /// <param name="func">目标Ease函数</param>
        /// <param name="exp">强度</param>
        /// <param name="period">周期</param>
        /// <param name="TotalFrame">总帧数</param>
        void UpdateAnimation(std::function<double(double t, double exp, double period)> func,double exp,double period)
        {
            if (frame <= totalframe)
            {
                double t = (double)frame / totalframe;
                value = start + (end - start) * func(t, exp, period);
                frame++;
            }
        }
    };

    struct easevalue2
    {
        easevalue x, y;
    };
    struct easevalue3
    {
        easevalue x, y,z;
    };
    struct easevalue4
    {
        easevalue x, y,w,h;
    };
    using EaseValue = easevalue;
    using EaseValue2 = easevalue2;
    using EaseValue3 = easevalue3;
    using EaseValue4 = easevalue4;

	//动画缓动函数
    namespace XEase {
        //基本缓动函数
        namespace EaseBasic
        {
            // 线性缓动：匀速运动，无加速度 : . . . . . . . . . . . . . . .
            double linear(double t);

            // ==================== 幂次缓动 ====================
            // 立方/幂次缓动（easeIn）：加速运动，开始慢，后面快 : .      .      .    .   .   . . . ...
            double easeIn(double t, double exp = 3.0);
            // 幂次缓动（easeOut）：减速运动，开始快，后面慢 : ... . . .   .   .    .      .      .
            double easeOut(double t, double exp = 3.0);
            // 幂次缓动（easeInOut）：先慢后快再慢 : .    .   . . . . . . . .   .    .
            double easeInOut(double t, double exp = 3.0);

            // ==================== 弹性缓动 ====================
            // 弹性缓动（easeInElastic）：开始时有弹性回拉，逐渐加速 : .  .   .    .     .      .   . . . . .
            double easeInElastic(double t, double amplitude = 1.0, double period = 0.3);
            // 弹性缓动（easeOutElastic）：结束时有弹性回拉，逐渐减速 : . . . . .   .      .     .    .   .  .
            double easeOutElastic(double t, double amplitude = 1.0, double period = 0.3);
            // 弹性缓动（easeInOutElastic）：两端弹性回拉 : .  .   .    . . . . . . . .    .   .  .
            double easeInOutElastic(double t, double amplitude = 1.0, double period = 0.45);

            // ==================== 回弹缓动 ====================
            // Back 缓动（easeInBack）：开始时先回拉再前进 :   .    .   . . . . . . . . . .
            double easeInBack(double t, double s = 1.70158);
            // Back 缓动（easeOutBack）：结束时超出目标再回弹 : . . . . . . . . .   .    .
            double easeOutBack(double t, double s = 1.70158);
            // Back 缓动（easeInOutBack）：两端回弹 :   .   . . . . . . . . .   .   .
            double easeInOutBack(double t, double s = 1.70158);

            // ==================== 弹跳缓动 ====================
            // Bounce 缓动（easeOutBounce）：像球落地弹跳，逐渐停止 : . . . . . . . . . . . . . . .
            double easeOutBounce(double t, int bounces = 3, double height = 0.5);
            // Bounce 缓动（easeInBounce）：反向弹跳，像球反弹上升 : . . . . . . . . . . . . . . .
            double easeInBounce(double t, int bounces = 3, double height = 0.5);
            // Bounce 缓动（easeInOutBounce）：两端弹跳 : . . . . . . . . . . . . . . .
            double easeInOutBounce(double t, int bounces = 3, double height = 0.5);

            // ==================== 正弦缓动 ====================
            // 正弦缓动（easeInSine）：开始慢，后面快，正弦曲线 : .    .   . . . . . . . . .
            double easeInSine(double t);
            // 正弦缓动（easeOutSine）：开始快，后面慢，正弦曲线 : . . . . . . . .   .    .
            double easeOutSine(double t);
            // 正弦缓动（easeInOutSine）：先慢后快再慢，正弦曲线 : .   . . . . . . . .   .
            double easeInOutSine(double t);

            // ==================== 指数缓动 ====================
            // 指数缓动（easeInExpo）：开始极慢，后面极快 : .         .        .      .   . . . . .
            double easeInExpo(double t);
            // 指数缓动（easeOutExpo）：开始极快，后面极慢 : . . . . .   .      .        .         .
            double easeOutExpo(double t);
            // 指数缓动（easeInOutExpo）：两端极慢，中间极快 : .     . . . . . . . . .     .
            double easeInOutExpo(double t);

            // ==================== 圆形缓动 ====================
            // 圆形缓动（easeInCirc）：开始慢，后面快，圆弧曲线 : .    .   . . . . . . . .
            double easeInCirc(double t);
            // 圆形缓动（easeOutCirc）：开始快，后面慢，圆弧曲线 : . . . . . . . .   .    .
            double easeOutCirc(double t);
            // 圆形缓动（easeInOutCirc）：两端慢，中间快，圆弧曲线 : .   . . . . . . . .   .
            double easeInOutCirc(double t);
        };

        double EaseAnimationTranslate(const String& AnimationType, int Frame, int TotalFrame);
    };

};