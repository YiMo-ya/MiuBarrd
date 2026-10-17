# MiuBarrd

**轻量 · 快捷 · 硬核**

一款基于 SFML 构建的轻量级电子白板，专注于极速渲染与极简交互，
通过灵活的插件系统赋予无限可能。

> [FREE] 完全免费，开箱即用，无激活码、无内购、无花里胡哨。

###软件截图
<img width="1914" height="1073" alt="0" src="https://github.com/user-attachments/assets/0d79145a-f38a-4331-9f14-8439c9c62b45" />

## 使用MiuBarrd
在Release链接中下载最新的zip压缩包后解压并运行MiuBarrd.exe即可，或者点击<a href="https://github.com/YiMo-ya/MiuBarrd/releases/latest">这里</a>下载最新版本</br>
<注意：需要C++Release包的环境>

---

## 技术架构

| 层级 | 技术 | 说明 |
|------|------|------|
| 渲染引擎 | SFML 2D Graphics | 硬件加速，高性能 2D 渲染 |
| 底层框架 | SFML | 跨平台多媒体库，处理窗口、图形、输入、音频 |
| 插件系统 | 动态加载模块 | 运行时加载/卸载，扩展功能无需重新编译 |

---

## 核心特性

### 渲染性能

- **批处理渲染管线** — 基于 SFML 的绘制优化，减少状态切换
- **自动批合并** — 合并相邻绘制调用，降低 draw call
- **硬件加速** — 支持 VBO / VAO，充分利用 GPU 能力
- **4K支持** — 保证4K屏幕的高分辨率的流畅

### 交互体验

- **智能化操作** — 手势识别、多指触控，书写与擦除行云流水
- **极简设计** — 无干扰界面，打开即用，专注内容本身
- **低延迟** — 笔迹渲染延迟控制在毫秒级

<img width="1919" height="1079" alt="1" src="https://github.com/user-attachments/assets/c57dd571-3d43-4271-9c1c-dc4ac6e0ac20" />
<img width="1919" height="1079" alt="2" src="https://github.com/user-attachments/assets/b32bd612-fd49-485a-91e1-b44ac0d0a7dc" />
<img width="1919" height="1079" alt="3" src="https://github.com/user-attachments/assets/8a0cb640-7a64-4471-a996-21826ad3e9c9" />

---

## 开发路线图

|方向 | 状态 |
|------|------|
| 优化体积，进一步轻量化 | 计划中 |
| 跨平台支持（Linux / macOS） | 计划中（目前仅支持 Windows 10+） |
| 接入 AI 高级功能 | 计划中 |


---

## 致谢

感谢每一位支持 MiuBarrd 的用户——学生、教师、开发者，
以及所有贡献者。你们的反馈与参与是这个项目持续前进的动力。

---
