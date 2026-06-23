#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <chrono>

namespace EdgeAI {

class CameraUtils {
public:
    // Image processing utilities
    static cv::Mat convertColorSpace(const cv::Mat& image, int code);
    static cv::Mat resizeImage(const cv::Mat& image, int width, int height);
    static cv::Mat cropImage(const cv::Mat& image, cv::Rect roi);
    static cv::Mat rotateImage(const cv::Mat& image, double angle);
    static cv::Mat flipImage(const cv::Mat& image, int flip_code);
    
    // Image enhancement
    static cv::Mat adjustBrightness(const cv::Mat& image, double alpha, double beta);
    static cv::Mat adjustContrast(const cv::Mat& image, double contrast);
    static cv::Mat adjustSharpness(const cv::Mat& image, double sharpness);
    static cv::Mat adjustWhiteBalance(const cv::Mat& image);
    static cv::Mat autoWhiteBalance(const cv::Mat& image);
    static cv::Mat histogramEqualization(const cv::Mat& image);
    static cv::Mat gammaCorrection(const cv::Mat& image, double gamma);
    static cv::Mat denoise(const cv::Mat& image, double h = 3.0);
    static cv::Mat edgeEnhancement(const cv::Mat& image);
    
    // Image analysis
    static double calculateBrightness(const cv::Mat& image);
    static double calculateContrast(const cv::Mat& image);
    static cv::Mat calculateHistogram(const cv::Mat& image);
    static bool isImageBlurry(const cv::Mat& image, double threshold = 100.0);
    static bool isImageTooDark(const cv::Mat& image, double threshold = 50.0);
    static bool isImageTooBright(const cv::Mat& image, double threshold = 200.0);
    static double calculatePSNR(const cv::Mat& img1, const cv::Mat& img2);
    static double calculateSSIM(const cv::Mat& img1, const cv::Mat& img2);
    
    // Image encoding/decoding
    static std::vector<uint8_t> encodeImage(const cv::Mat& image, 
                                            const std::string& format = ".jpg",
                                            int quality = 95);
    static cv::Mat decodeImage(const std::vector<uint8_t>& data);
    static bool saveImage(const cv::Mat& image, const std::string& path, 
                          int quality = 95);
    static cv::Mat loadImage(const std::string& path);
    static bool saveImageWithMetadata(const cv::Mat& image, 
                                      const std::string& path,
                                      const std::map<std::string, std::string>& metadata);
    
    // Image stitching and mosaicing
    static std::vector<cv::Mat> splitImage(const cv::Mat& image, 
                                           int rows, int cols);
    static cv::Mat stitchImages(const std::vector<cv::Mat>& images, 
                                int cols, int rows);
    static cv::Mat createPanorama(const std::vector<cv::Mat>& images);
    
    // Video processing
    static double calculateFPS(const std::vector<cv::Mat>& frames);
    static std::vector<cv::Mat> extractFrames(const std::string& video_path, 
                                              int skip_frames = 0);
    static bool createVideo(const std::vector<cv::Mat>& frames, 
                            const std::string& output_path, 
                            double fps = 30.0);
    static cv::Mat addTextOverlay(const cv::Mat& image, 
                                  const std::string& text,
                                  cv::Point position,
                                  double font_scale = 1.0,
                                  cv::Scalar color = cv::Scalar(0, 255, 0));
    
    // Camera calibration
    static cv::Mat calibrateCamera(const std::vector<cv::Mat>& images);
    static cv::Mat undistortImage(const cv::Mat& image, 
                                  const cv::Mat& camera_matrix,
                                  const cv::Mat& distortion_coeffs);
    static cv::Mat applyLensCorrection(const cv::Mat& image, double strength = 1.0);
    
    // Feature detection
    static std::vector<cv::KeyPoint> detectSIFT(const cv::Mat& image);
    static std::vector<cv::KeyPoint> detectORB(const cv::Mat& image);
    static std::vector<cv::KeyPoint> detectFAST(const cv::Mat& image);
    static cv::Mat drawKeypoints(const cv::Mat& image, 
                                 const std::vector<cv::KeyPoint>& keypoints);
    
    // Object detection utilities
    static cv::Mat drawBoundingBoxes(const cv::Mat& image,
                                     const std::vector<cv::Rect>& boxes,
                                     const std::vector<std::string>& labels = {},
                                     const std::vector<cv::Scalar>& colors = {});
    static cv::Mat drawSegmentationMask(const cv::Mat& image,
                                        const cv::Mat& mask,
                                        cv::Scalar color = cv::Scalar(0, 255, 0),
                                        double alpha = 0.5);
    static cv::Mat applyHeatmap(const cv::Mat& image, 
                                const cv::Mat& heatmap);
    
    // Timestamp utilities
    static std::string getTimestamp();
    static int64_t getTimestampMilliseconds();
    static std::string formatTimestamp(int64_t timestamp);
    
    // Image metadata
    struct ImageMetadata {
        int width;
        int height;
        int channels;
        int depth;
        double brightness;
        double contrast;
        double entropy;
        std::string format;
        size_t size_bytes;
    };
    static ImageMetadata getImageMetadata(const cv::Mat& image);
    
    // Utility functions
    static void printImageInfo(const cv::Mat& image, const std::string& name = "");
    static cv::Mat createTestPattern(int width, int height, int pattern_type = 0);
    static cv::Mat blendImages(const cv::Mat& img1, const cv::Mat& img2, 
                               double alpha = 0.5);
    static cv::Mat applyFilter(const cv::Mat& image, const std::string& filter_type);
    static cv::Mat applyColormap(const cv::Mat& image, int colormap_type);
};

} // namespace EdgeAI
