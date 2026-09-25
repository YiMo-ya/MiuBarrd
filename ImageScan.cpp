#include "ImageScan.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ImgScan
{
    using namespace std;

    namespace
    {

        // ============================================================ 着色器源码
        constexpr const char* kFragmentShader = R"(
uniform sampler2D uSource;
uniform sampler2D uBg;
uniform mat3      uH;
uniform vec2      uOutSize;
uniform vec2      uSrcSize;
uniform float     uSharp;

uniform float uBlack;
uniform float uWhite;
uniform float uGamma;
uniform float uMaskEnable;
uniform float uMaskLow;
uniform float uMaskHigh;
uniform float uFeather;
uniform float uColorKeep;

// 新增：中性灰参考，用于白平衡（在CPU端计算并传入）
uniform vec3 uNeutralGray;

void main()
{
    vec2 uv = vec2(gl_FragCoord.x / uOutSize.x, 1.0 - gl_FragCoord.y / uOutSize.y);

    vec3 hp  = uH * vec3(uv, 1.0);
    vec2 suv = hp.xy / hp.z;
    if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) {
        gl_FragColor = vec4(0.0);
        return;
    }

    vec3  c = texture2D(uSource, suv).rgb;

    float bg = texture2D(uBg, suv).r;
    
    // ---- 核心修复：白平衡 + 亮度归一化 ----
    // 1. 使用中性灰参考做白平衡，消除纸张色偏。
    //    关键：白平衡只能改「颜色」，不能改「整体亮度」。
    //    直接用 c / uNeutralGray 会把整幅图按 1/uNeutralGray 倍增益放大，
    //    当纸张亮度不足 1.0 时（如 0.5）就相当于整体曝光 x2，导致严重过曝。
    //    因此先除以中性灰，再乘回中性灰自身的亮度，把整体增益拉回 1.0，
    //    只保留逐通道的比例（色偏校正）效果。
    vec3  grayRef   = max(uNeutralGray, vec3(1e-4));
    float refLuma   = dot(grayRef, vec3(0.299, 0.587, 0.114));
    vec3  balancedC = c / grayRef * max(refLuma, 1e-4);
    
    // 2. 重新计算白平衡后的亮度
    float balancedL = dot(balancedC, vec3(0.299, 0.587, 0.114));
    
    // 3. 亮度归一化（平场校正）
    //    用逐像素亮度除以背景光照图，消除纸面明暗不均（中心亮、四角暗）。
    //    这里 bg 是低频光照分布，比值本身已把整体亮度归一到 1.0 附近，
    //    不要再引入额外增益，否则又会被推亮。
    float n = balancedL / max(bg, 0.02);

    // 4. 对比度映射
    //    修好两件事：亮度可控、过渡平滑。
    //    原先这一串级联（×1.45 → smoothstep(0.15,0.82) → 提白 +0.35）
    //    会把亮度一路推高且失真；而窄区间的 smoothstep 会放大光照残差，
    //    表现为「斑块 / 色阶断层」，也就是用户说的「一点也不平滑」。
    //    现在只保留一次线性黑白场拉伸 + 一次 gamma，全部是单调连续映射：
    //    - blackPoint 以下压成纯黑，whitePoint 以上提成纯白（尊重参数语义）；
    //    - 中间为线性过渡，不再叠加额外增益，亮度不再漂移；
    //    - 不再串联任何窄 smoothstep，避免放大 bg 的低频残差。
    float v = clamp((n - uBlack) / max(uWhite - uBlack, 1e-3), 0.0, 1.0);
    v = pow(v, uGamma);

    // 6. 颜色恢复
    // 关键修复：不能再用 balancedC * (v / balancedL) 做颜色恢复。
    // 原因：v/balancedL 是「逐像素亮度增益」，当某个彩色像素本身很暗
    // （如纯红 balancedC 的亮度 balancedL 很小），而对比度映射把 v 推高后，
    // 该增益会非常大，使主色通道撞上 clamp(1.0) 饱和、其余通道被压低，
    // 于是红色变成极深红、蓝色变成极浅蓝——颜色被推向极端。
    //
    // 正确做法：把原色拆成「灰度 + 色度偏移」，用映射后的亮度 v 作为新的灰度，
    // 再叠加原始色度偏移（保持原图饱和度）。这样亮度严格等于 v，色相与
    // 饱和度都不受增益失控影响，彩色不会失真为黑白或极端色。
    vec3  chromaOffset = balancedC - vec3(balancedL);   // 纯色度分量（可正可负）
    vec3  tint = clamp(vec3(v) + chromaOffset, 0.0, 1.0);
    
    vec3 gray = vec3(v);
    // 冷调：蓝通道略抬、红通道略压，幅度很小（约 1.5%）
    vec3 coolDoc = vec3(gray.r * 0.992, gray.g * 0.998, min(1.0, gray.b * 1.012));
    
    // uColorKeep=1 时保留白平衡后的原色，0 时退化为冷调灰阶
    vec3 rgb  = mix(coolDoc, tint, uColorKeep);

    float mask = mix(1.0, smoothstep(uMaskLow, uMaskHigh, bg), uMaskEnable);

    // 7. 饱和度增强（替代原先的锐化）
    //    在保持亮度不变的前提下拉开颜色与灰度的差距：
    //    先求当前亮度，再按系数把每个通道相对灰度推远，
    //    系数 1.0 = 原样，>1 = 更鲜艳。这里取 1.15 做温和提升。
    //    注意：只在颜色通道上做，纯灰像素的 RGB 相等，饱和度处理不会改变它，
    //    因此不会引入额外亮度（避免再次过曝）。
    float satLuma = dot(rgb, vec3(0.299, 0.587, 0.114));
    rgb = clamp(mix(vec3(satLuma), rgb, 1.15), 0.0, 1.0);

    gl_FragColor = vec4(rgb, mask);
}
)";

        // ============================================================ 基础算法

        inline float Luma(uint8_t r, uint8_t g, uint8_t b)
        {
            return (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
        }

        int OtsuThreshold(const int* hist, int total)
        {
            if (total <= 0) return 128;

            double sum = 0.0;
            for (int i = 0; i < 256; ++i)
                sum += double(i) * double(hist[i]);

            double sumB = 0.0;
            int    wB = 0;
            double best = -1.0;
            int    thr = 128;

            for (int t = 0; t < 256; ++t)
            {
                wB += hist[t];
                if (wB == 0) continue;

                const int wF = total - wB;
                if (wF == 0) break;

                sumB += double(t) * double(hist[t]);
                const double mB = sumB / wB;
                const double mF = (sum - sumB) / wF;
                const double between = double(wB) * double(wF) * (mB - mF) * (mB - mF);

                if (between > best) { best = between; thr = t; }
            }
            return thr;
        }

        bool Solve8(double a[8][9], double x[8])
        {
            for (int col = 0; col < 8; ++col)
            {
                int piv = col;
                for (int r = col + 1; r < 8; ++r)
                    if (fabs(a[r][col]) > fabs(a[piv][col]))
                        piv = r;

                if (fabs(a[piv][col]) < 1e-12) return false;

                if (piv != col)
                    for (int c = col; c < 9; ++c)
                        swap(a[col][c], a[piv][c]);

                const double inv = 1.0 / a[col][col];
                for (int c = col; c < 9; ++c)
                    a[col][c] *= inv;

                for (int r = 0; r < 8; ++r)
                {
                    if (r == col) continue;
                    const double f = a[r][col];
                    if (f == 0.0) continue;
                    for (int c = col; c < 9; ++c)
                        a[r][c] -= f * a[col][c];
                }
            }
            for (int i = 0; i < 8; ++i)
                x[i] = a[i][8];
            return true;
        }

        bool ComputeHomography(const Vector2f dst[4], const Vector2f src[4], float out[9])
        {
            double a[8][9] = {};
            for (int i = 0; i < 4; ++i)
            {
                const double x = dst[i].x, y = dst[i].y;
                const double u = src[i].x, v = src[i].y;
                double* r0 = a[i * 2];
                double* r1 = a[i * 2 + 1];
                r0[0] = x; r0[1] = y; r0[2] = 1.0; r0[6] = -x * u; r0[7] = -y * u; r0[8] = u;
                r1[3] = x; r1[4] = y; r1[5] = 1.0; r1[6] = -x * v; r1[7] = -y * v; r1[8] = v;
            }

            double s[8];
            if (!Solve8(a, s)) return false;

            out[0] = float(s[0]); out[1] = float(s[3]); out[2] = float(s[6]);
            out[3] = float(s[1]); out[4] = float(s[4]); out[5] = float(s[7]);
            out[6] = float(s[2]); out[7] = float(s[5]); out[8] = 1.0f;
            return true;
        }

        // 新增：计算图像中性灰（用于白平衡）
        // 取图像中亮度最高的 5% 像素的平均值作为中性灰参考
        Vector3f ComputeNeutralGray(const Image& img)
        {
            const Vector2u sz = img.getSize();
            const uint8_t* px = img.getPixelsPtr();

            vector<float> brightPixels;
            brightPixels.reserve(sz.x * sz.y);

            for (unsigned y = 0; y < sz.y; ++y)
            {
                for (unsigned x = 0; x < sz.x; ++x)
                {
                    const uint8_t* p = px + (size_t(y) * sz.x + x) * 4;
                    float luma = Luma(p[0], p[1], p[2]);
                    brightPixels.push_back(luma);
                }
            }

            // 排序取最亮的 5%
            size_t k = brightPixels.size() * 0.95f;
            nth_element(brightPixels.begin(), brightPixels.begin() + k, brightPixels.end());

            float sumLuma = 0.0f;
            for (size_t i = k; i < brightPixels.size(); ++i)
            {
                sumLuma += brightPixels[i];
            }
            float avgBrightLuma = sumLuma / (brightPixels.size() - k);

            // 假设最亮区域是白纸，其 RGB 应该相等
            // 这里简化为用亮度平均值来估算中性灰
            // 更精确的做法是取最亮像素的 RGB 平均值
            return Vector3f(avgBrightLuma, avgBrightLuma, avgBrightLuma);
        }

        bool DetectPaperQuad(const Image& img, Vector2f outQuad[4])
        {
            const Vector2u sz = img.getSize();
            if (sz.x < 16 || sz.y < 16) return false;

            const unsigned shrink = max(1u, max(sz.x, sz.y) / 256u);
            const unsigned w = sz.x / shrink;
            const unsigned h = sz.y / shrink;
            if (w < 8 || h < 8) return false;

            const uint8_t* px = img.getPixelsPtr();

            vector<uint8_t> gray(size_t(w) * h);
            int hist[256] = {};

            for (unsigned y = 0; y < h; ++y)
            {
                for (unsigned x = 0; x < w; ++x)
                {
                    const uint8_t* p = px + (size_t(y * shrink) * sz.x + x * shrink) * 4;
                    const auto v = static_cast<uint8_t>(Luma(p[0], p[1], p[2]) * 255.0f);
                    gray[size_t(y) * w + x] = v;
                    ++hist[v];
                }
            }

            const int thr = OtsuThreshold(hist, int(w) * int(h));

            vector<uint8_t> visited(size_t(w) * h, 0);
            vector<int> stack, comp, best;
            const int dx4[4] = { -1, 1, 0, 0 };
            const int dy4[4] = { 0, 0, -1, 1 };

            for (unsigned y = 0; y < h; ++y)
            {
                for (unsigned x = 0; x < w; ++x)
                {
                    const int seed = int(y * w + x);
                    if (gray[seed] <= thr || visited[seed]) continue;

                    comp.clear();
                    stack.assign(1, seed);
                    visited[seed] = 1;

                    while (!stack.empty())
                    {
                        const int cur = stack.back();
                        stack.pop_back();
                        comp.push_back(cur);

                        const int cx = cur % int(w);
                        const int cy = cur / int(w);

                        for (int k = 0; k < 4; ++k)
                        {
                            const int nx = cx + dx4[k];
                            const int ny = cy + dy4[k];
                            if (nx < 0 || ny < 0 || nx >= int(w) || ny >= int(h)) continue;
                            const int ni = ny * int(w) + nx;
                            if (gray[ni] > thr && !visited[ni])
                            {
                                visited[ni] = 1;
                                stack.push_back(ni);
                            }
                        }
                    }

                    if (comp.size() > best.size())
                        best.swap(comp);
                }
            }

            const float ratio = float(best.size()) / float(size_t(w) * h);
            if (ratio < 0.05f || ratio > 0.995f) return false;

            vector<Vector2f> pts;
            pts.reserve(best.size());
            for (int idx : best)
            {
                const float x = float(idx % int(w));
                const float y = float(idx / int(w));
                pts.push_back({ x, y });
            }

            if (pts.size() < 4) return false;

            sort(pts.begin(), pts.end(),
                [](const Vector2f& a, const Vector2f& b)
                {
                    if (a.x != b.x) return a.x < b.x;
                    return a.y < b.y;
                });

            const auto cross = [](const Vector2f& a, const Vector2f& b, const Vector2f& c)
                {
                    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
                };

            vector<Vector2f> hull;
            hull.reserve(pts.size() * 2);
            for (size_t i = 0; i < pts.size(); ++i)
            {
                while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull.back(), pts[i]) <= 0.0f)
                    hull.pop_back();
                hull.push_back(pts[i]);
            }
            const size_t lower = hull.size();
            for (size_t i = pts.size() - 1; i-- > 0;)
            {
                while (hull.size() > lower && cross(hull[hull.size() - 2], hull.back(), pts[i]) <= 0.0f)
                    hull.pop_back();
                hull.push_back(pts[i]);
            }
            if (!hull.empty()) hull.pop_back();

            if (hull.size() < 4) return false;

            const auto quadArea = [](const Vector2f& a, const Vector2f& b,
                const Vector2f& c, const Vector2f& d)
                {
                    const float area = a.x * b.y - b.x * a.y
                        + b.x * c.y - c.x * b.y
                        + c.x * d.y - d.x * c.y
                        + d.x * a.y - a.x * d.y;
                    return fabs(area) * 0.5f;
                };

            const size_t m = hull.size();
            int bi[4] = { 0, 0, 0, 0 };
            float bestArea = -1.0f;

            for (size_t i = 0; i < m; ++i)
                for (size_t j = i + 1; j < m; ++j)
                    for (size_t k = j + 1; k < m; ++k)
                        for (size_t l = k + 1; l < m; ++l)
                        {
                            const float area = quadArea(hull[i], hull[j], hull[k], hull[l]);
                            if (area > bestArea)
                            {
                                bestArea = area;
                                bi[0] = int(i); bi[1] = int(j); bi[2] = int(k); bi[3] = int(l);
                            }
                        }

            Vector2f corner[4] = { hull[bi[0]], hull[bi[1]], hull[bi[2]], hull[bi[3]] };

            int start = 0;
            float bestCorner = 1e9f;
            for (int i = 0; i < 4; ++i)
            {
                const float s = corner[i].x + corner[i].y;
                if (s < bestCorner) { bestCorner = s; start = i; }
            }

            Vector2f center = { 0.0f, 0.0f };
            for (int i = 0; i < 4; ++i)
                center += corner[i];
            center *= 0.25f;
            const Vector2f centerPx = center * float(shrink);

            const float expandRatio = 1.03f;
            const float expandPx = 6.0f;

            for (int i = 0; i < 4; ++i)
            {
                const Vector2f& c = corner[(start + i) % 4];
                const Vector2f px = { c.x * float(shrink), c.y * float(shrink) };

                const Vector2f dir = { px.x - centerPx.x, px.y - centerPx.y };
                const float len = sqrt(dir.x * dir.x + dir.y * dir.y);
                if (len < 1e-3f)
                {
                    outQuad[i] = px;
                    continue;
                }

                const float grow = max((expandRatio - 1.0f) * len, expandPx);
                outQuad[i] = { px.x + dir.x / len * grow, px.y + dir.y / len * grow };
            }
            return true;
        }

        Image BuildBackgroundMap(const Image& img)
        {
            const Vector2u sz = img.getSize();
            const uint8_t* px = img.getPixelsPtr();

            const unsigned gw = clamp(sz.x / 24u, 8u, 160u);
            const unsigned gh = clamp(sz.y / 24u, 8u, 160u);

            vector<float> buf(size_t(gw) * gh, 0.0f);
            vector<float> samples;
            samples.reserve(64);

            for (unsigned gy = 0; gy < gh; ++gy)
            {
                for (unsigned gx = 0; gx < gw; ++gx)
                {
                    const unsigned x0 = unsigned(uint64_t(gx) * sz.x / gw);
                    const unsigned x1 = unsigned(uint64_t(gx + 1) * sz.x / gw);
                    const unsigned y0 = unsigned(uint64_t(gy) * sz.y / gh);
                    const unsigned y1 = unsigned(uint64_t(gy + 1) * sz.y / gh);

                    const unsigned stepX = max(1u, (x1 - x0) / 8u);
                    const unsigned stepY = max(1u, (y1 - y0) / 8u);

                    samples.clear();
                    for (unsigned y = y0; y < y1; y += stepY)
                        for (unsigned x = x0; x < x1; x += stepX)
                        {
                            const uint8_t* p = px + (size_t(y) * sz.x + x) * 4;
                            samples.push_back(Luma(p[0], p[1], p[2]));
                        }

                    if (samples.empty())
                    {
                        buf[size_t(gy) * gw + gx] = 0.0f;
                        continue;
                    }

                    const size_t k = (samples.size() * 9) / 10;
                    nth_element(samples.begin(), samples.begin() + k, samples.end());
                    buf[size_t(gy) * gw + gx] = samples[k];
                }
            }

            const int R = 3;
            vector<float> tmp(buf.size());

            for (int pass = 0; pass < 3; ++pass)
            {
                for (unsigned y = 0; y < gh; ++y)
                    for (unsigned x = 0; x < gw; ++x)
                    {
                        float acc = 0.0f;
                        int   n = 0;
                        for (int k = -R; k <= R; ++k)
                        {
                            const int xx = clamp(int(x) + k, 0, int(gw) - 1);
                            acc += buf[size_t(y) * gw + xx];
                            ++n;
                        }
                        tmp[size_t(y) * gw + x] = acc / float(n);
                    }

                for (unsigned y = 0; y < gh; ++y)
                    for (unsigned x = 0; x < gw; ++x)
                    {
                        float acc = 0.0f;
                        int   n = 0;
                        for (int k = -R; k <= R; ++k)
                        {
                            const int yy = clamp(int(y) + k, 0, int(gh) - 1);
                            acc += tmp[size_t(yy) * gw + x];
                            ++n;
                        }
                        buf[size_t(y) * gw + x] = acc / float(n);
                    }
            }

            Image out(Vector2u{ gw, gh });
            for (unsigned y = 0; y < gh; ++y)
                for (unsigned x = 0; x < gw; ++x)
                {
                    const float v = clamp(buf[size_t(y) * gw + x], 0.0f, 1.0f);
                    const auto  b = static_cast<uint8_t>(v * 255.0f + 0.5f);
                    out.setPixel({ x, y }, Color(b, b, b));
                }
            return out;
        }

    } // namespace

    // ============================================================ 对外接口

    bool Correct(Texture& tex, const Options& opt)
    {
        if (!Shader::isAvailable())
            return false;

        const Vector2u srcSize = tex.getSize();
        if (srcSize.x == 0 || srcSize.y == 0)
            return false;

        // ---- 1. 取回源像素（检测与建背景图都基于它）----
        const Image src = tex.copyToImage();

        // ---- 2. 检测纸张四边形；失败就退化成“整幅图”，只做色调矫正 ----
        Vector2f quad[4] = {
            { 0.0f, 0.0f },
            { float(srcSize.x), 0.0f },
            { float(srcSize.x), float(srcSize.y) },
            { 0.0f, float(srcSize.y) }
        };

        if (opt.enableWarp)
        {
            Vector2f detected[4];
            if (!DetectPaperQuad(src, detected))
                return false;          // 检测失败：直接放弃，tex 保持不变
            for (int i = 0; i < 4; ++i)
                quad[i] = detected[i];
        }

        // ---- 3. 输出尺寸：取对边平均长度，保证长宽比不被拉伸 ----
        const auto dist = [](const Vector2f& a, const Vector2f& b)
            {
                const float dx = a.x - b.x;
                const float dy = a.y - b.y;
                return sqrt(dx * dx + dy * dy);
            };

        const unsigned outW = unsigned(max(1.0f, (dist(quad[0], quad[1]) + dist(quad[3], quad[2])) * 0.5f));
        const unsigned outH = unsigned(max(1.0f, (dist(quad[0], quad[3]) + dist(quad[1], quad[2])) * 0.5f));

        // ---- 4. 单应矩阵：目标归一化坐标 -> 源归一化坐标 ----
        const Vector2f dstN[4] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        Vector2f srcN[4];
        for (int i = 0; i < 4; ++i)
            srcN[i] = { quad[i].x / float(srcSize.x), quad[i].y / float(srcSize.y) };

        float hm[9] = {};
        if (!ComputeHomography(dstN, srcN, hm))
            return false;

        // ---- 5. 背景/光照图 ----
        Texture bgTex;
        if (!bgTex.loadFromImage(BuildBackgroundMap(src)))
            return false;
        bgTex.setSmooth(true);   // 低分辨率图必须开线性过滤，否则会看到块状

        // 源纹理走线性过滤，配合着色器锐化，避免放大时出现阶梯感
        tex.setSmooth(true);

        // ---- 6. 着色器只编译一次 ----
        static Shader shader;
        static bool       shaderReady = false;
        if (!shaderReady)
        {
            shaderReady = shader.loadFromMemory(kFragmentShader, Shader::Type::Fragment);
            if (!shaderReady)
                return false;
        }

        shader.setUniform("uSource", tex);
        shader.setUniform("uBg", bgTex);
        shader.setUniform("uH", Glsl::Mat3(hm));   // 注意：SFML 的 Mat3 是列主序
        shader.setUniform("uOutSize", Vector2f(float(outW), float(outH)));
        shader.setUniform("uSrcSize", Vector2f(float(srcSize.x), float(srcSize.y)));
        shader.setUniform("uSharp", 0.45f);
        shader.setUniform("uBlack", opt.blackPoint);
        shader.setUniform("uWhite", opt.whitePoint);
        shader.setUniform("uGamma", opt.gamma);
        shader.setUniform("uMaskEnable", opt.enableMask ? 1.0f : 0.0f);
        shader.setUniform("uMaskLow", opt.maskLow);
        shader.setUniform("uMaskHigh", opt.maskHigh);
        shader.setUniform("uFeather", opt.featherPx);
        shader.setUniform("uColorKeep", opt.colorKeep);

        // ---- 7. 离屏渲染 ----
        RenderTexture rt(Vector2u{ outW, outH });
        rt.clear(Color::Transparent);

        // 全屏四边形；具体采哪里由 uH 决定，所以顶点纹理坐标随便给
        VertexArray cover(PrimitiveType::TriangleStrip, 4);
        cover[0] = Vertex({ 0.0f, 0.0f });
        cover[1] = Vertex({ float(outW), 0.0f });
        cover[2] = Vertex({ 0.0f, float(outH) });
        cover[3] = Vertex({ float(outW), float(outH) });

        RenderStates states;
        states.shader = &shader;
        states.blendMode = BlendNone;   // 直接覆写，保留着色器算出的 alpha

        rt.draw(cover, states);
        rt.display();

        // ---- 8. 写回原纹理（“直接修改传入的 Texture&”）----
        // 走 copyToImage 而不是 update(const Texture&)：RenderTexture 的纹理带 Y 翻转标记，
        // GPU↔GPU 直拷会上下颠倒，copyToImage() 会替我们把方向摆正。
        if (!tex.resize({ outW, outH }))
            return false;
        tex.update(rt.getTexture().copyToImage());
        return true;
    }

} // namespace ImgScan
