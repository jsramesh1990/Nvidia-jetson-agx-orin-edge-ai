#pragma once
#include <opencv2/opencv.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <queue>

namespace EdgeAI {

class CameraPipeline {
public:
    CameraPipeline();
    ~CameraPipeline();
    
    bool buildPipeline(const std::string& pipeline_string);
    bool start();
    void stop();
    bool isRunning() const;
    
    cv::Mat getFrame();
    bool setProperty(const std::string& name, const std::string& value);
    bool setProperty(const std::string& name, int value);
    bool setProperty(const std::string& name, double value);
    
    // GStreamer specific
    GstElement* getElement(const std::string& name);
    void setAppsinkCallback(std::function<void(GstSample*)> callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
