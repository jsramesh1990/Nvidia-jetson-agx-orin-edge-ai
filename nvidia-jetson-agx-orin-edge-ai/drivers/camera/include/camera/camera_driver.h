#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <functional>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

namespace EdgeAI {

enum CameraType {
    CAMERA_CSI = 0,
    CAMERA_USB = 1,
    CAMERA_IP = 2,
    CAMERA_FILE = 3
};

enum CameraResolution {
    RES_640x480 = 0,
    RES_1280x720 = 1,
    RES_1920x1080 = 2,
    RES_3840x2160 = 3,
    RES_4096x2160 = 4
};

enum CameraFPS {
    FPS_15 = 15,
    FPS_30 = 30,
    FPS_60 = 60,
    FPS_120 = 120
};

struct CameraConfig {
    CameraType type = CAMERA_CSI;
    CameraResolution resolution = RES_1920x1080;
    CameraFPS fps = FPS_30;
    std::string device = "/dev/video0";
    std::string ip_address = "";
    int port = 554;
    std::string username = "";
    std::string password = "";
    std::string file_path = "";
    bool auto_focus = true;
    int exposure = -1;  // -1 for auto
    int white_balance = -1;  // -1 for auto
    int brightness = 50;
    int contrast = 50;
    int saturation = 50;
    int sharpness = 50;
    bool flip_horizontal = false;
    bool flip_vertical = false;
    int rotation = 0;  // 0, 90, 180, 270
    std::string pipeline = "nvarguscamerasrc";
    int buffer_size = 4;
    int timeout_ms = 5000;
};

struct CameraFrame {
    cv::Mat image;
    int64_t timestamp;
    int frame_id;
    double processing_time_ms;
    std::string source;
    std::vector<uint8_t> raw_data;
};

struct CameraStats {
    int total_frames = 0;
    int dropped_frames = 0;
    double avg_fps = 0.0;
    double min_processing_time = 0.0;
    double max_processing_time = 0.0;
    double avg_processing_time = 0.0;
};

class CameraDriver {
public:
    CameraDriver(const CameraConfig& config);
    ~CameraDriver();
    
    // Initialization
    bool initialize();
    bool startStream();
    void stopStream();
    bool restartStream();
    bool isRunning() const;
    
    // Frame capture
    cv::Mat captureFrame();
    CameraFrame captureFrameWithMetadata();
    bool captureFrameAsync(std::function<void(const CameraFrame&)> callback);
    std::vector<cv::Mat> captureFrames(int count);
    
    // Configuration
    void setResolution(CameraResolution resolution);
    void setFPS(CameraFPS fps);
    void setExposure(int value);
    void setWhiteBalance(int value);
    void setBrightness(int value);
    void setContrast(int value);
    void setSaturation(int value);
    void setSharpness(int value);
    void setFocus(int value);
    void setZoom(float zoom);
    void setROI(cv::Rect roi);
    void setFlip(bool horizontal, bool vertical);
    void setRotation(int degrees);
    
    // Status
    CameraConfig getConfig() const;
    CameraStats getStats() const;
    bool isFrameReady() const;
    int getFrameWidth() const;
    int getFrameHeight() const;
    int getFPS() const;
    
    // Callbacks
    void setFrameCallback(std::function<void(const CameraFrame&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
    void setStatsCallback(std::function<void(const CameraStats&)> callback);
    
    // Utility
    bool saveSnapshot(const std::string& path);
    bool recordVideo(const std::string& path, int duration_seconds);
    std::string getCameraInfo() const;
    bool testCamera();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
