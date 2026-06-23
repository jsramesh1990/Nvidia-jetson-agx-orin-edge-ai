#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <map>
#include <string>

namespace EdgeAI {

struct FaceDescriptor {
    std::vector<float> embedding;
    std::string user_id;
    std::string name;
};

class FaceRecognizer {
public:
    FaceRecognizer(const std::string& model_path);
    ~FaceRecognizer();
    
    FaceDescriptor extractFeature(const cv::Mat& face_image);
    std::string recognize(const FaceDescriptor& descriptor);
    void addUser(const std::string& user_id, 
                 const std::string& name,
                 const std::vector<FaceDescriptor>& descriptors);
    bool deleteUser(const std::string& user_id);
    void saveDatabase(const std::string& path);
    void loadDatabase(const std::string& path);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
