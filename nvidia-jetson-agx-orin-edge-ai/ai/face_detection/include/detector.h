#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace EdgeAI {

struct FaceBox {
    cv::Rect bbox;
    float confidence;
    std::vector<cv::Point2f> landmarks;
};

class FaceDetector {
public:
    FaceDetector(const std::string& model_path);
    ~FaceDetector();
    
    std::vector<FaceBox> detect(const cv::Mat& image);
    void setConfidenceThreshold(float threshold);
    void setInputSize(int width, int height);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
