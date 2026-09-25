#pragma once
#include "Xs.Main.h"

namespace xs
{
    struct Entry
    {
        sf::PrimitiveType type;
        std::vector<sf::Vertex> vertices;
    };

    extern std::vector<Entry> s_entries;

    enum BatchType
    {
		BATCH_AUTO,//自动
		BATCH_HANDLED//手动
    };

    namespace XBatch
    {
        /// <summary>
		/// 开始批处理
        /// </summary>
        void begin(RenderTarget& target);
        /// <summary>
		/// 结束批处理
        /// </summary>
        /// <param name="target">渲染目标</param>
        void end();

        /// <summary>
		/// 是否正在批处理
        /// </summary>
        /// <returns>返回是否正在批处理</returns>
        bool isBatching();
        /// <summary>
        /// 获取批处理类型
        /// </summary>
        /// <returns>返回批处理类型</returns>
        BatchType GetBatchType();
		/// <summary>
		/// 设置批处理类型。
		/// </summary>
		/// <param name="type">批处理类型，BATCH_AUTO（自动）或 BATCH_HANDLED（手动）</param>
        /// <param name="警告">如果有不同绘制目标，比如离屏渲染时建议手动。自动Batch并不会执行最正确的批处理命令</param>
		void SetBatchType(BatchType type);

        // 检查并修正当前目标（内部函数）
        void CheckDest(RenderTarget& Dest);

        // TriangleStrip（矩形 / 线段）（内部函数）
        void addQuad(
            const sf::Vector2f& p0,
            const sf::Vector2f& p1,
            const sf::Vector2f& p2,
            const sf::Vector2f& p3,
            const sf::Color& color
        );

        // TriangleFan（圆 / 弧 / 点）（内部函数）
        void addTriangleFan(
            const std::vector<sf::Vector2f>& fan,
            const sf::Color& color
        );
    };
};