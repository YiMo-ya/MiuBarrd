#pragma once
#include "CameraQR.h"
#include "qrcodegen.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace qr {

    // ------------------------------------------------------------
    // 将任意文本编码为标准二维码并渲染成 SFML 纹理。
    // 采用 Nayuki qrcodegen（ISO/IEC 18004 完整实现）：
    // 自动选择最小版本、自动评估 8 种掩码、完整绘制定位/时序/对齐/格式图形，
    // 因此生成的二维码可被任意标准扫码器识别。
    // ------------------------------------------------------------

    /// <summary>
    /// 将文本编码为标准二维码并渲染为 SFML 纹理。
    /// </summary>
    /// <param name="text">待编码的文本（URL / ASCII，UTF-8 字节序列）。</param>
    /// <param name="scale">每个模块的像素边长，过小会导致扫码失败，建议不小于 4。</param>
    /// <param name="border">四周静默区宽度（单位：模块），QR 规范要求至少 4。</param>
    /// <returns>可直接绘制的二维码纹理。</returns>
    static Texture generateTexture(const std::string& text,
        int scale = 8,
        int border = 4) {
        // MEDIUM 纠错：投屏 URL 场景下兼顾容量与容错能力
        const qrcodegen::QrCode code =
            qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);

        const int modules = code.getSize();
        const int dim = modules + border * 2;   // 含静默区的总模块数
        const int px = dim * scale;             // 最终像素边长

        // 初始化为纯白：白色模块与静默区可直接复用该底色，无需再写入
        std::vector<uint8_t> pixels(static_cast<size_t>(px) * px * 4, 0xFF);

        for (int y = 0; y < px; ++y) {
            for (int x = 0; x < px; ++x) {
                // 像素坐标换算为模块坐标；border 区域内 mx/my 为负，
                // 落入此范围的像素保持白色，自然形成静默区
                const int mx = x / scale - border;
                const int my = y / scale - border;
                const bool dark = (mx >= 0 && my >= 0 && mx < modules && my < modules)
                    && code.getModule(mx, my);
                if (!dark)
                    continue;  // 白色已由初始值保证，跳过以省去一次写入

                const int ix = (y * px + x) * 4;
                pixels[ix + 0] = 0x00;
                pixels[ix + 1] = 0x00;
                pixels[ix + 2] = 0x00;
                pixels[ix + 3] = 0xFF;
            }
        }

        Image img;
        // SFML 3.0 正确写法：直接用像素数据 resize
        img.resize({ static_cast<unsigned>(px), static_cast<unsigned>(px) }, pixels.data());

        Texture tex;
        tex.loadFromImage(img);
        // 关闭平滑：插值会模糊模块边界，直接导致扫码器无法识别
        tex.setSmooth(false);
        return tex;
    }

} // namespace qr

Texture QR::GetQR(std::string text)
{
    return qr::generateTexture(text);
}
