#pragma once
#include "CameraQR.h"
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

namespace qr {

    // ------------------------------------------------------------
    // 极简 QR 编码器（Version 6，纠错 M，支持字节模式）
    // 仅用于 URL / ASCII，不适合长 HTML
    // ------------------------------------------------------------
    struct QRCode {
        static constexpr int VERSION = 6;
        static constexpr int MODULES = 21 + (VERSION - 1) * 4; // 41
        static constexpr int TOTAL_CODEWORDS = 172;
        static constexpr int ECC_CODEWORDS = 48; // M level

        uint8_t modules[MODULES][MODULES] = {};
        uint8_t mask = 0;

        void setModule(int x, int y, bool v) {
            if (x >= 0 && y >= 0 && x < MODULES && y < MODULES)
                modules[y][x] = v;
        }

        bool getModule(int x, int y) const {
            return modules[y][x];
        }
    };// ------------------------------------------------------------
    // 工具函数
    // ------------------------------------------------------------
    static uint8_t galMul(uint8_t a, uint8_t b) {
        uint8_t p = 0;
        for (int i = 0; i < 8; ++i) {
            if (b & 1) p ^= a;
            bool hiBit = a & 0x80;
            a <<= 1;
            if (hiBit) a ^= 0x1D;
            b >>= 1;
        }
        return p;
    }

    // ------------------------------------------------------------
    // 生成多项式
    // ------------------------------------------------------------
    static void rsGenerateECC(const uint8_t* data, int dataLen,
        uint8_t* ecc, int eccLen) {
        std::vector<uint8_t> gen(eccLen + 1, 0);
        gen[0] = 1;
        for (int i = 0; i < eccLen; ++i) {
            uint8_t coeff = 1 << i;
            for (int j = i + 1; j > 0; --j) {
                gen[j] = gen[j - 1] ^ galMul(gen[j], coeff);
            }
            gen[0] = galMul(gen[0], coeff);
        }

        std::vector<uint8_t> remainder(dataLen + eccLen, 0);
        std::memcpy(remainder.data(), data, dataLen);

        for (int i = 0; i < dataLen; ++i) {
            uint8_t factor = remainder[i];
            if (factor) {
                for (int j = 0; j < eccLen + 1; ++j) {
                    remainder[i + j] ^= galMul(gen[j], factor);
                }
            }
        }

        std::memcpy(ecc, remainder.data() + dataLen, eccLen);
    }

    // ------------------------------------------------------------
    // 核心编码（字节模式）
    // ------------------------------------------------------------
    static QRCode encode(const std::string& text) {
        QRCode qr;

        // 模式指示（字节模式）
        std::vector<uint8_t> bits;
        auto pushBits = [&](uint32_t val, int len) {
            for (int i = len - 1; i >= 0; --i)
                bits.push_back((val >> i) & 1);
            };

        pushBits(0b0100, 4); // 字节模式
        pushBits(text.size(), 8);

        for (char c : text) {
            pushBits(static_cast<uint8_t>(c), 8);
        }

        // 终止符
        pushBits(0, 4);

        // 补齐到 8 的倍数
        while (bits.size() % 8 != 0) bits.push_back(0);

        // 转为字节
        std::vector<uint8_t> data;
        for (size_t i = 0; i < bits.size(); i += 8) {
            uint8_t b = 0;
            for (int j = 0; j < 8; ++j) b = (b << 1) | bits[i + j];
            data.push_back(b);
        }

        // 填充码字
        static const uint8_t pad[] = { 0xEC, 0x11 };
        int idx = 0;
        while (data.size() < QRCode::TOTAL_CODEWORDS - QRCode::ECC_CODEWORDS) {
            data.push_back(pad[idx++ % 2]);
        }

        // 生成 ECC
        uint8_t ecc[QRCode::ECC_CODEWORDS];
        rsGenerateECC(data.data(), data.size(), ecc, QRCode::ECC_CODEWORDS);

        // 交错
        std::vector<uint8_t> finalData;
        for (size_t i = 0; i < data.size(); ++i) finalData.push_back(data[i]);
        for (int i = 0; i < QRCode::ECC_CODEWORDS; ++i) finalData.push_back(ecc[i]);

        // 放置模块（简化版：只画数据区域，实际 QR 需要位置探测图形）
        //  这里是简化实现，完整版需要添加：
        // - 位置探测图形（三个角）
        // - 时序线
        // - 格式信息
        // - 掩码处理

        // 为了让你先跑通，这里用最简化的方式：
        int x = 0, y = qr.MODULES - 1, dir = -1;
        for (size_t i = 0; i < finalData.size(); ++i) {
            for (int bit = 7; bit >= 0; --bit) {
                qr.setModule(x, y, (finalData[i] >> bit) & 1);
                y += dir;
                if (y < 0 || y >= qr.MODULES) {
                    y -= dir;
                    x++;
                    dir = -dir;
                }
            }
        }

        return qr;
    }

    // ------------------------------------------------------------
    // 生成 SFML Texture
    // ------------------------------------------------------------
    static Texture generateTexture(const std::string& text,
        int scale = 8,
        int border = 4) {
        QRCode qr = encode(text);

        int dim = qr.MODULES + border * 2;
        int px = dim * scale;

        std::vector<uint8_t> pixels(px * px * 4, 0);

        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                bool dark =
                    (x >= border && y >= border &&
                        x < border + qr.MODULES && y < border + qr.MODULES)
                    ? qr.getModule(x - border, y - border)
                    : false;

                uint8_t v = dark ? 0x00 : 0xFF;

                int startX = x * scale;
                int startY = y * scale;

                for (int dy = 0; dy < scale; ++dy) {
                    for (int dx = 0; dx < scale; ++dx) {
                        int ix = (startY + dy) * px + (startX + dx);
                        pixels[ix * 4 + 0] = v;
                        pixels[ix * 4 + 1] = v;
                        pixels[ix * 4 + 2] = v;
                        pixels[ix * 4 + 3] = 255;
                    }
                }
            }
        }

        Image img;
        // SFML 3.0 正确写法：直接用像素数据 resize
        img.resize({ static_cast<unsigned>(px), static_cast<unsigned>(px) }, pixels.data());

        Texture tex;
        tex.loadFromImage(img);
        tex.setSmooth(false);
        return tex;
    }

} // namespace qr

Texture QR::GetQR(std::string text)
{
    return qr::generateTexture(text);
}