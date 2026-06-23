#include "camera/camera_pipeline.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>

namespace EdgeAI {

class CameraPipeline::Impl {
public:
    Impl() : pipeline_(nullptr), bus_(nullptr), running_(false), 
             appsink_(nullptr), frame_counter_(0) {
        // Initialize GStreamer if not already done
        static bool gst_initialized = false;
        if (!gst_initialized) {
            gst_init(NULL, NULL);
            gst_initialized = true;
        }
    }
    
    ~Impl() {
        stop();
        if (pipeline_) {
            gst_object_unref(pipeline_);
            pipeline_ = nullptr;
        }
    }
    
    bool buildPipeline(const std::string& pipeline_string) {
        // Stop any existing pipeline
        stop();
        
        // Create pipeline from string
        GError* error = nullptr;
        pipeline_ = gst_parse_launch(pipeline_string.c_str(), &error);
        
        if (error) {
            std::cerr << "Failed to build pipeline: " << error->message << std::endl;
            g_error_free(error);
            return false;
        }
        
        if (!pipeline_) {
            std::cerr << "Failed to create pipeline" << std::endl;
            return false;
        }
        
        // Get appsink element
        appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
        if (!appsink_) {
            std::cerr << "No appsink element found in pipeline" << std::endl;
            gst_object_unref(pipeline_);
            pipeline_ = nullptr;
            return false;
        }
        
        // Configure appsink
        gst_app_sink_set_drop(GST_APP_SINK(appsink_), true);
        gst_app_sink_set_max_buffers(GST_APP_SINK(appsink_), 2);
        gst_app_sink_set_emit_signals(GST_APP_SINK(appsink_), true);
        
        // Get bus for messages
        bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
        
        return true;
    }
    
    bool start() {
        if (!pipeline_) {
            std::cerr << "Pipeline not built" << std::endl;
            return false;
        }
        
        if (running_) {
            return true;
        }
        
        GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
        
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Failed to start pipeline" << std::endl;
            return false;
        }
        
        running_ = true;
        
        // Start bus monitoring thread
        bus_thread_ = std::thread(&Impl::busMonitorLoop, this);
        
        // Start frame capture thread if callback is set
        if (frame_callback_) {
            capture_thread_ = std::thread(&Impl::captureLoop, this);
        }
        
        return true;
    }
    
    void stop() {
        if (!running_) {
            return;
        }
        
        running_ = false;
        
        if (pipeline_) {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
        }
        
        if (bus_thread_.joinable()) {
            bus_thread_.join();
        }
        
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
        
        // Clear frame queue
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!frame_queue_.empty()) {
            GstSample* sample = frame_queue_.front();
            frame_queue_.pop();
            gst_sample_unref(sample);
        }
    }
    
    bool isRunning() const {
        return running_;
    }
    
    cv::Mat getFrame() {
        if (!running_ || !appsink_) {
            return cv::Mat();
        }
        
        // Check if we have frames in queue first
        GstSample* sample = nullptr;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (!frame_queue_.empty()) {
                sample = frame_queue_.front();
                frame_queue_.pop();
            }
        }
        
        // If no frame in queue, try to get one directly
        if (!sample) {
            sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink_));
        }
        
        if (!sample) {
            return cv::Mat();
        }
        
        cv::Mat frame = gstSampleToMat(sample);
        gst_sample_unref(sample);
        
        if (!frame.empty()) {
            frame_counter_++;
        }
        
        return frame;
    }
    
    bool setProperty(const std::string& name, const std::string& value) {
        if (!pipeline_) {
            return false;
        }
        
        GstElement* element = getElement(name);
        if (!element) {
            return false;
        }
        
        g_object_set(G_OBJECT(element), "property", value.c_str(), NULL);
        return true;
    }
    
    bool setProperty(const std::string& name, int value) {
        if (!pipeline_) {
            return false;
        }
        
        GstElement* element = getElement(name);
        if (!element) {
            return false;
        }
        
        g_object_set(G_OBJECT(element), "property", value, NULL);
        return true;
    }
    
    bool setProperty(const std::string& name, double value) {
        if (!pipeline_) {
            return false;
        }
        
        GstElement* element = getElement(name);
        if (!element) {
            return false;
        }
        
        g_object_set(G_OBJECT(element), "property", value, NULL);
        return true;
    }
    
    GstElement* getElement(const std::string& name) {
        if (!pipeline_) {
            return nullptr;
        }
        
        return gst_bin_get_by_name(GST_BIN(pipeline_), name.c_str());
    }
    
    void setAppsinkCallback(std::function<void(GstSample*)> callback) {
        appsink_callback_ = callback;
        
        if (callback && appsink_) {
            // Connect to the new-sample signal
            g_signal_connect(appsink_, "new-sample", 
                             G_CALLBACK(onNewSample), this);
        }
    }
    
private:
    GstElement* pipeline_;
    GstElement* appsink_;
    GstBus* bus_;
    std::atomic<bool> running_;
    std::thread bus_thread_;
    std::thread capture_thread_;
    std::atomic<int> frame_counter_;
    std::mutex queue_mutex_;
    std::queue<GstSample*> frame_queue_;
    std::function<void(GstSample*)> appsink_callback_;
    std::function<void(const cv::Mat&)> frame_callback_;
    
    static GstFlowReturn onNewSample(GstAppSink* sink, gpointer user_data) {
        Impl* impl = static_cast<Impl*>(user_data);
        
        GstSample* sample = gst_app_sink_pull_sample(sink);
        if (!sample) {
            return GST_FLOW_ERROR;
        }
        
        // Store sample in queue
        std::lock_guard<std::mutex> lock(impl->queue_mutex_);
        impl->frame_queue_.push(sample);
        
        // Call user callback if set
        if (impl->appsink_callback_) {
            impl->appsink_callback_(sample);
        }
        
        // Keep only last 2 frames to prevent memory issues
        while (impl->frame_queue_.size() > 2) {
            GstSample* old_sample = impl->frame_queue_.front();
            impl->frame_queue_.pop();
            gst_sample_unref(old_sample);
        }
        
        return GST_FLOW_OK;
    }
    
    void busMonitorLoop() {
        while (running_) {
            GstMessage* msg = gst_bus_timed_pop_filtered(
                bus_, 
                GST_MSECOND * 100,
                static_cast<GstMessageType>(GST_MESSAGE_ERROR | 
                                            GST_MESSAGE_EOS | 
                                            GST_MESSAGE_STATE_CHANGED)
            );
            
            if (!msg) {
                continue;
            }
            
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError* err = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_error(msg, &err, &debug);
                    
                    std::cerr << "Pipeline error: " << err->message << std::endl;
                    if (debug) {
                        std::cerr << "Debug info: " << debug << std::endl;
                        g_free(debug);
                    }
                    g_error_free(err);
                    
                    // Attempt to recover
                    if (running_) {
                        gst_element_set_state(pipeline_, GST_STATE_READY);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
                    }
                    break;
                }
                case GST_MESSAGE_EOS: {
                    std::cerr << "End of stream" << std::endl;
                    break;
                }
                case GST_MESSAGE_STATE_CHANGED: {
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline_)) {
                        GstState old_state, new_state, pending_state;
                        gst_message_parse_state_changed(msg, &old_state, 
                                                        &new_state, &pending_state);
                        // std::cout << "Pipeline state changed: " 
                        //           << gst_element_state_get_name(old_state) 
                        //           << " -> " 
                        //           << gst_element_state_get_name(new_state) 
                        //           << std::endl;
                    }
                    break;
                }
                default:
                    break;
            }
            
            gst_message_unref(msg);
        }
    }
    
    void captureLoop() {
        while (running_) {
            cv::Mat frame = getFrame();
            if (!frame.empty() && frame_callback_) {
                frame_callback_(frame);
            }
            
            // Don't consume all CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    cv::Mat gstSampleToMat(GstSample* sample) {
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        if (!buffer) {
            return cv::Mat();
        }
        
        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            return cv::Mat();
        }
        
        // Get caps to determine dimensions
        GstCaps* caps = gst_sample_get_caps(sample);
        if (!caps) {
            gst_buffer_unmap(buffer, &map);
            return cv::Mat();
        }
        
        GstStructure* structure = gst_caps_get_structure(caps, 0);
        if (!structure) {
            gst_buffer_unmap(buffer, &map);
            gst_caps_unref(caps);
            return cv::Mat();
        }
        
        // Get width and height
        int width, height;
        if (!gst_structure_get_int(structure, "width", &width) ||
            !gst_structure_get_int(structure, "height", &height)) {
            gst_buffer_unmap(buffer, &map);
            gst_caps_unref(caps);
            return cv::Mat();
        }
        
        // Determine format
        const char* format = gst_structure_get_string(structure, "format");
        cv::Mat mat;
        
        if (format && strcmp(format, "BGR") == 0) {
            // Already BGR format
            mat = cv::Mat(height, width, CV_8UC3, map.data).clone();
        } else if (format && strcmp(format, "RGB") == 0) {
            // Convert RGB to BGR
            cv::Mat temp(height, width, CV_8UC3, map.data);
            cv::cvtColor(temp, mat, cv::COLOR_RGB2BGR);
        } else if (format && strcmp(format, "NV12") == 0) {
            // NV12 to BGR using CUDA/NVMM conversion
            // This is handled by nvvidconv in pipeline, but we handle it here too
            cv::Mat nv12(height * 3 / 2, width, CV_8UC1, map.data);
            cv::cvtColor(nv12, mat, cv::COLOR_YUV2BGR_NV12);
        } else if (format && strcmp(format, "I420") == 0) {
            // I420 to BGR
            cv::Mat i420(height * 3 / 2, width, CV_8UC1, map.data);
            cv::cvtColor(i420, mat, cv::COLOR_YUV2BGR_I420);
        } else if (format && strcmp(format, "GRAY8") == 0) {
            // Grayscale to BGR
            cv::Mat gray(height, width, CV_8UC1, map.data);
            cv::cvtColor(gray, mat, cv::COLOR_GRAY2BGR);
        } else if (format && strcmp(format, "GRAY16_LE") == 0) {
            // 16-bit grayscale to 8-bit BGR
            cv::Mat gray16(height, width, CV_16UC1, map.data);
            double min_val, max_val;
            cv::minMaxLoc(gray16, &min_val, &max_val);
            gray16.convertTo(gray16, CV_8UC1, 255.0 / (max_val - min_val), 
                           -min_val * 255.0 / (max_val - min_val));
            cv::cvtColor(gray16, mat, cv::COLOR_GRAY2BGR);
        } else {
            // Unknown format, try to convert to BGR
            std::cerr << "Unknown GStreamer format: " << (format ? format : "NULL") << std::endl;
            gst_buffer_unmap(buffer, &map);
            gst_caps_unref(caps);
            return cv::Mat();
        }
        
        gst_buffer_unmap(buffer, &map);
        gst_caps_unref(caps);
        
        return mat;
    }
};

// CameraPipeline implementation
CameraPipeline::CameraPipeline() : pImpl(std::make_unique<Impl>()) {}
CameraPipeline::~CameraPipeline() = default;

bool CameraPipeline::buildPipeline(const std::string& pipeline_string) {
    return pImpl->buildPipeline(pipeline_string);
}

bool CameraPipeline::start() {
    return pImpl->start();
}

void CameraPipeline::stop() {
    pImpl->stop();
}

bool CameraPipeline::isRunning() const {
    return pImpl->isRunning();
}

cv::Mat CameraPipeline::getFrame() {
    return pImpl->getFrame();
}

bool CameraPipeline::setProperty(const std::string& name, const std::string& value) {
    return pImpl->setProperty(name, value);
}

bool CameraPipeline::setProperty(const std::string& name, int value) {
    return pImpl->setProperty(name, value);
}

bool CameraPipeline::setProperty(const std::string& name, double value) {
    return pImpl->setProperty(name, value);
}

GstElement* CameraPipeline::getElement(const std::string& name) {
    return pImpl->getElement(name);
}

void CameraPipeline::setAppsinkCallback(std::function<void(GstSample*)> callback) {
    pImpl->setAppsinkCallback(callback);
}

} // namespace EdgeAI
