#pragma once
#include "Xs/Xs.h"
#include <cstdint>
#include <string>
#include <vector>

namespace cam {

    struct DeviceInfo {
        int         index = -1;
        std::string name;
        std::string id;
    };

    struct CameraConfig {
        std::string device = "video=USB Camera";
        std::string format = "dshow";
        int         width = 640;
        int         height = 480;
        int         fps = 30;
    };

    class Camera {
    public:
        explicit Camera(const CameraConfig& cfg);
        ~Camera();

        Camera(const Camera&) = delete;
        Camera& operator=(const Camera&) = delete;

        bool start();
        void stop();
        bool isRunning() const;

        int  width()  const;
        int  height() const;

        bool setWidth(int width);
        bool setHeight(int height);

        bool applyConfig(const CameraConfig& cfg);

        // 更新纹理，返回是否成功
        bool update(sf::Texture& tex);
        bool setDevice(int index);



        // 设备枚举
        static CameraConfig               makeConfig(int index,
            int w = 640, int h = 480, int fps = 30);
        static std::vector<DeviceInfo>    listDevices();

    private:
        struct Impl;
        Impl* p_;
    };

} // namespace cam

class CameraManager
{
public:
    static xs::EV Rotation;

    static void DrawCameraImage(int x, int y, float Scale,RenWin& window);

    static bool Init(int index, int w = 1920, int h = 1080);

    static void Stop();

    static void SleepUpdate(int time);

    static int GetW();
    static int GetH();

    static void Get(xs::IMAGE& img);

    static void Rote();

    static bool NeedAutoPhoto();
};