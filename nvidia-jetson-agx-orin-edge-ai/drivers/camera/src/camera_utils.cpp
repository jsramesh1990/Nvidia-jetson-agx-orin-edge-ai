#include "camera/camera_utils.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace EdgeAI {

// Image processing utilities
cv::Mat CameraUtils::convertColorSpace(const cv::Mat& image, int code) {
    cv::Mat result;
    cv::cvtColor(image, result, code);
    return result;
}

cv::Mat CameraUtils::resizeImage(const cv::Mat& image, int width, int height) {
    cv::Mat result;
    cv::resize(image, result, cv::Size(width, height));
    return result;
}

cv::Mat CameraUtils::cropImage(const cv::Mat& image, cv::Rect roi) {
    return image(roi).clone();
}

cv::Mat CameraUtils::rotateImage(const cv::Mat& image, double angle) {
    cv::Mat result;
    cv::Point2f center(image.cols / 2.0f, image.rows / 2.0f);
    cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::warpAffine(image, result, rot, image.size());
    return result;
}

cv::Mat CameraUtils::flipImage(const cv::Mat& image, int flip_code) {
    cv::Mat result;
    cv::flip(image, result, flip_code);
    return result;
}

// Image enhancement
cv::Mat CameraUtils::adjustBrightness(const cv::Mat& image, double alpha, double beta) {
    cv::Mat result;
    image.convertTo(result, -1, alpha, beta);
    return result;
}

cv::Mat CameraUtils::adjustContrast(const cv::Mat& image, double contrast) {
    cv::Mat result;
    image.convertTo(result, -1, 1.0 + contrast/100.0, 0);
    return result;
}

cv::Mat CameraUtils::adjustSharpness(const cv::Mat& image, double sharpness) {
    cv::Mat result;
    cv::Mat kernel = (cv::Mat_<float>(3,3) << 
        0, -1, 0,
        -1, 4 + sharpness * 2, -1,
        0, -1, 0);
    cv::filter2D(image, result, -1, kernel);
    return result;
}

cv::Mat CameraUtils::adjustWhiteBalance(const cv::Mat& image) {
    // Simple white balance using gray world assumption
    cv::Mat result;
    cv::Scalar mean = cv::mean(image);
    double avg = (mean[0] + mean[1] + mean[2]) / 3.0;
    double scale_b = avg / mean[0];
    double scale_g = avg / mean[1];
    double scale_r = avg / mean[2];
    
    std::vector<cv::Mat> channels;
    cv::split(image, channels);
    channels[0] *= scale_b;
    channels[1] *= scale_g;
    channels[2] *= scale_r;
    cv::merge(channels, result);
    return result;
}

cv::Mat CameraUtils::autoWhiteBalance(const cv::Mat& image) {
    // More sophisticated white balance
    cv::Mat result;
    cv::Mat lab;
    cv::cvtColor(image, lab, cv::COLOR_BGR2Lab);
    
    std::vector<cv::Mat> lab_channels;
    cv::split(lab, lab_channels);
    
    // Apply CLAHE to L channel
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(lab_channels[0], lab_channels[0]);
    
    cv::merge(lab_channels, lab);
    cv::cvtColor(lab, result, cv::COLOR_Lab2BGR);
    return result;
}

cv::Mat CameraUtils::histogramEqualization(const cv::Mat& image) {
    cv::Mat result;
    if (image.channels() == 1) {
        cv::equalizeHist(image, result);
    } else {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        for (auto& channel : channels) {
            cv::equalizeHist(channel, channel);
        }
        cv::merge(channels, result);
    }
    return result;
}

cv::Mat CameraUtils::gammaCorrection(const cv::Mat& image, double gamma) {
    cv::Mat result;
    cv::Mat lookUpTable(1, 256, CV_8U);
    uchar* p = lookUpTable.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, gamma) * 255.0);
    }
    cv::LUT(image, lookUpTable, result);
    return result;
}

cv::Mat CameraUtils::denoise(const cv::Mat& image, double h) {
    cv::Mat result;
    cv::fastNlMeansDenoisingColored(image, result, h, h, 7, 21);
    return result;
}

cv::Mat CameraUtils::edgeEnhancement(const cv::Mat& image) {
    cv::Mat gray, edges, result;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::Canny(gray, edges, 100, 200);
    cv::cvtColor(edges, edges, cv::COLOR_GRAY2BGR);
    cv::addWeighted(image, 0.9, edges, 0.1, 0, result);
    return result;
}

// Image analysis
double CameraUtils::calculateBrightness(const cv::Mat& image) {
    cv::Scalar mean = cv::mean(image);
    return (mean[0] + mean[1] + mean[2]) / 3.0;
}

double CameraUtils::calculateContrast(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }
    cv::Scalar mean, stddev;
    cv::meanStdDev(gray, mean, stddev);
    return stddev[0];
}

cv::Mat CameraUtils::calculateHistogram(const cv::Mat& image) {
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::Mat hist;
    
    if (image.channels() == 1) {
        cv::calcHist(&image, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
    } else {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        cv::Mat hist_b, hist_g, hist_r;
        cv::calcHist(&channels[0], 1, 0, cv::Mat(), hist_b, 1, &histSize, &histRange);
        cv::calcHist(&channels[1], 1, 0, cv::Mat(), hist_g, 1, &histSize, &histRange);
        cv::calcHist(&channels[2], 1, 0, cv::Mat(), hist_r, 1, &histSize, &histRange);
        
        // Merge histograms
        cv::vconcat(hist_b, hist_g, hist);
        cv::vconcat(hist, hist_r, hist);
    }
    return hist;
}

bool CameraUtils::isImageBlurry(const cv::Mat& image, double threshold) {
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }
    
    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_64F);
    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev);
    double variance = stddev[0] * stddev[0];
    return variance < threshold;
}

bool CameraUtils::isImageTooDark(const cv::Mat& image, double threshold) {
    return calculateBrightness(image) < threshold;
}

bool CameraUtils::isImageTooBright(const cv::Mat& image, double threshold) {
    return calculateBrightness(image) > threshold;
}

double CameraUtils::calculatePSNR(const cv::Mat& img1, const cv::Mat& img2) {
    cv::Mat diff;
    cv::absdiff(img1, img2, diff);
    diff.convertTo(diff, CV_64F);
    diff = diff.mul(diff);
    cv::Scalar s = cv::sum(diff);
    double mse = s[0] / (img1.total() * img1.channels());
    if (mse <= 1e-10) return 100.0;
    return 10.0 * log10(255.0 * 255.0 / mse);
}

double CameraUtils::calculateSSIM(const cv::Mat& img1, const cv::Mat& img2) {
    const double C1 = 6.5025, C2 = 58.5225;
    
    cv::Mat I1, I2;
    img1.convertTo(I1, CV_64F);
    img2.convertTo(I2, CV_64F);
    
    cv::Mat mu1, mu2, sigma1_sq, sigma2_sq, sigma12;
    cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);
    
    cv::Mat mu1_sq = mu1.mul(mu1);
    cv::Mat mu2_sq = mu2.mul(mu2);
    cv::Mat mu1_mu2 = mu1.mul(mu2);
    
    cv::GaussianBlur(I1.mul(I1), sigma1_sq, cv::Size(11, 11), 1.5);
    sigma1_sq -= mu1_sq;
    
    cv::GaussianBlur(I2.mul(I2), sigma2_sq, cv::Size(11, 11), 1.5);
    sigma2_sq -= mu2_sq;
    
    cv::GaussianBlur(I1.mul(I2), sigma12, cv::Size(11, 11), 1.5);
    sigma12 -= mu1_mu2;
    
    cv::Mat t1 = 2 * mu1_mu2 + C1;
    cv::Mat t2 = 2 * sigma12 + C2;
    cv::Mat t1_sq = mu1_sq + mu2_sq + C1;
    cv::Mat t2_sq = sigma1_sq + sigma2_sq + C2;
    
    cv::Mat ssim_map;
    cv::divide(t1.mul(t2), t1_sq.mul(t2_sq), ssim_map);
    cv::Scalar mssim = cv::mean(ssim_map);
    return mssim[0];
}

// Image encoding/decoding
std::vector<uint8_t> CameraUtils::encodeImage(const cv::Mat& image, 
                                              const std::string& format,
                                              int quality) {
    std::vector<int> params;
    if (format == ".jpg" || format == ".jpeg") {
        params.push_back(cv::IMWRITE_JPEG_QUALITY);
        params.push_back(quality);
    } else if (format == ".png") {
        params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        params.push_back(9);
    }
    
    std::vector<uint8_t> buffer;
    cv::imencode(format, image, buffer, params);
    return buffer;
}

cv::Mat CameraUtils::decodeImage(const std::vector<uint8_t>& data) {
    return cv::imdecode(data, cv::IMREAD_COLOR);
}

bool CameraUtils::saveImage(const cv::Mat& image, const std::string& path, int quality) {
    std::vector<int> params;
    std::string ext = path.substr(path.find_last_of("."));
    
    if (ext == ".jpg" || ext == ".jpeg") {
        params.push_back(cv::IMWRITE_JPEG_QUALITY);
        params.push_back(quality);
    } else if (ext == ".png") {
        params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        params.push_back(9);
    }
    
    return cv::imwrite(path, image, params);
}

cv::Mat CameraUtils::loadImage(const std::string& path) {
    return cv::imread(path);
}

bool CameraUtils::saveImageWithMetadata(const cv::Mat& image, 
                                        const std::string& path,
                                        const std::map<std::string, std::string>& metadata) {
    // Save image
    if (!saveImage(image, path)) return false;
    
    // Save metadata to sidecar file
    std::string metadata_path = path + ".meta";
    std::ofstream file(metadata_path);
    if (!file.is_open()) return false;
    
    for (const auto& [key, value] : metadata) {
        file << key << ":" << value << "\n";
    }
    file.close();
    return true;
}

// Image stitching and mosaicing
std::vector<cv::Mat> CameraUtils::splitImage(const cv::Mat& image, 
                                             int rows, int cols) {
    std::vector<cv::Mat> tiles;
    int tile_width = image.cols / cols;
    int tile_height = image.rows / rows;
    
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            cv::Rect roi(c * tile_width, r * tile_height, 
                        tile_width, tile_height);
            tiles.push_back(image(roi).clone());
        }
    }
    return tiles;
}

cv::Mat CameraUtils::stitchImages(const std::vector<cv::Mat>& images, 
                                  int cols, int rows) {
    if (images.empty()) return cv::Mat();
    
    int tile_width = images[0].cols;
    int tile_height = images[0].rows;
    cv::Mat result(rows * tile_height, cols * tile_width, images[0].type());
    
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int idx = r * cols + c;
            if (idx < (int)images.size()) {
                images[idx].copyTo(result(cv::Rect(c * tile_width, 
                                                   r * tile_height,
                                                   tile_width, tile_height)));
            }
        }
    }
    return result;
}

cv::Mat CameraUtils::createPanorama(const std::vector<cv::Mat>& images) {
    if (images.empty()) return cv::Mat();
    
    cv::Ptr<cv::Stitcher> stitcher = cv::Stitcher::create(cv::Stitcher::PANORAMA);
    cv::Mat panorama;
    cv::Stitcher::Status status = stitcher->stitch(images, panorama);
    
    if (status != cv::Stitcher::OK) {
        return cv::Mat();
    }
    return panorama;
}

// Video processing
double CameraUtils::calculateFPS(const std::vector<cv::Mat>& frames) {
    if (frames.size() < 2) return 0.0;
    return 30.0; // Placeholder
}

std::vector<cv::Mat> CameraUtils::extractFrames(const std::string& video_path, 
                                                int skip_frames) {
    std::vector<cv::Mat> frames;
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) return frames;
    
    int frame_count = 0;
    cv::Mat frame;
    while (cap.read(frame)) {
        if (frame_count % (skip_frames + 1) == 0) {
            frames.push_back(frame.clone());
        }
        frame_count++;
    }
    cap.release();
    return frames;
}

bool CameraUtils::createVideo(const std::vector<cv::Mat>& frames, 
                              const std::string& output_path, 
                              double fps) {
    if (frames.empty()) return false;
    
    cv::VideoWriter writer(output_path,
                          cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                          fps,
                          frames[0].size());
    
    if (!writer.isOpened()) return false;
    
    for (const auto& frame : frames) {
        writer.write(frame);
    }
    writer.release();
    return true;
}

cv::Mat CameraUtils::addTextOverlay(const cv::Mat& image, 
                                    const std::string& text,
                                    cv::Point position,
                                    double font_scale,
                                    cv::Scalar color) {
    cv::Mat result = image.clone();
    cv::putText(result, text, position, cv::FONT_HERSHEY_SIMPLEX,
                font_scale, color, 2);
    return result;
}

// Camera calibration
cv::Mat CameraUtils::calibrateCamera(const std::vector<cv::Mat>& images) {
    // Implementation for camera calibration
    return cv::Mat();
}

cv::Mat CameraUtils::undistortImage(const cv::Mat& image, 
                                    const cv::Mat& camera_matrix,
                                    const cv::Mat& distortion_coeffs) {
    cv::Mat result;
    cv::undistort(image, result, camera_matrix, distortion_coeffs);
    return result;
}

cv::Mat CameraUtils::applyLensCorrection(const cv::Mat& image, double strength) {
    // Simple lens correction
    cv::Mat result;
    cv::Mat map_x, map_y;
    map_x.create(image.size(), CV_32FC1);
    map_y.create(image.size(), CV_32FC1);
    
    float cx = image.cols / 2.0f;
    float cy = image.rows / 2.0f;
    
    for (int y = 0; y < image.rows; y++) {
        for (int x = 0; x < image.cols; x++) {
            float dx = (x - cx) / cx;
            float dy = (y - cy) / cy;
            float r = sqrt(dx*dx + dy*dy);
            float correction = 1.0f + strength * r * r;
            map_x.at<float>(y,x) = cx + dx * correction * cx;
            map_y.at<float>(y,x) = cy + dy * correction * cy;
        }
    }
    
    cv::remap(image, result, map_x, map_y, cv::INTER_LINEAR);
    return result;
}

// Feature detection
std::vector<cv::KeyPoint> CameraUtils::detectSIFT(const cv::Mat& image) {
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
    std::vector<cv::KeyPoint> keypoints;
    sift->detect(image, keypoints);
    return keypoints;
}

std::vector<cv::KeyPoint> CameraUtils::detectORB(const cv::Mat& image) {
    cv::Ptr<cv::ORB> orb = cv::ORB::create();
    std::vector<cv::KeyPoint> keypoints;
    orb->detect(image, keypoints);
    return keypoints;
}

std::vector<cv::KeyPoint> CameraUtils::detectFAST(const cv::Mat& image) {
    std::vector<cv::KeyPoint> keypoints;
    cv::FAST(image, keypoints, 40);
    return keypoints;
}

cv::Mat CameraUtils::drawKeypoints(const cv::Mat& image, 
                                   const std::vector<cv::KeyPoint>& keypoints) {
    cv::Mat result;
    cv::drawKeypoints(image, keypoints, result);
    return result;
}

// Object detection utilities
cv::Mat CameraUtils::drawBoundingBoxes(const cv::Mat& image,
                                       const std::vector<cv::Rect>& boxes,
                                       const std::vector<std::string>& labels,
                                       const std::vector<cv::Scalar>& colors) {
    cv::Mat result = image.clone();
    
    for (size_t i = 0; i < boxes.size(); i++) {
        cv::Scalar color = (i < colors.size()) ? colors[i] : cv::Scalar(0, 255, 0);
        cv::rectangle(result, boxes[i], color, 2);
        
        if (i < labels.size()) {
            cv::putText(result, labels[i], boxes[i].tl(), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
        }
    }
    return result;
}

cv::Mat CameraUtils::drawSegmentationMask(const cv::Mat& image,
                                          const cv::Mat& mask,
                                          cv::Scalar color,
                                          double alpha) {
    cv::Mat result = image.clone();
    cv::Mat colored_mask;
    cv::cvtColor(mask, colored_mask, cv::COLOR_GRAY2BGR);
    colored_mask = colored_mask * 0.5 + cv::Scalar(0, 255, 0) * 0.5;
    cv::addWeighted(result, 1 - alpha, colored_mask, alpha, 0, result);
    return result;
}

cv::Mat CameraUtils::applyHeatmap(const cv::Mat& image, 
                                  const cv::Mat& heatmap) {
    cv::Mat result;
    cv::applyColorMap(heatmap, result, cv::COLORMAP_JET);
    cv::addWeighted(image, 0.5, result, 0.5, 0, result);
    return result;
}

// Timestamp utilities
std::string CameraUtils::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

int64_t CameraUtils::getTimestampMilliseconds() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

std::string CameraUtils::formatTimestamp(int64_t timestamp) {
    auto time_t = timestamp / 1000;
    auto ms = timestamp % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms;
    return ss.str();
}

// Image metadata
CameraUtils::ImageMetadata CameraUtils::getImageMetadata(const cv::Mat& image) {
    ImageMetadata metadata;
    metadata.width = image.cols;
    metadata.height = image.rows;
    metadata.channels = image.channels();
    metadata.depth = image.depth();
    metadata.brightness = calculateBrightness(image);
    metadata.contrast = calculateContrast(image);
    metadata.size_bytes = image.total() * image.elemSize();
    
    // Calculate entropy
    cv::Mat hist = calculateHistogram(image);
    double entropy = 0;
    for (int i = 0; i < hist.rows; i++) {
        double p = hist.at<float>(i) / (image.total() * image.channels());
        if (p > 0) {
            entropy += -p * log2(p);
        }
    }
    metadata.entropy = entropy;
    
    // Determine format
    metadata.format = (image.channels() == 1) ? "Grayscale" : "Color";
    
    return metadata;
}

void CameraUtils::printImageInfo(const cv::Mat& image, const std::string& name) {
    std::cout << "Image Info - " << name << ":" << std::endl;
    std::cout << "  Dimensions: " << image.cols << "x" << image.rows << std::endl;
    std::cout << "  Channels: " << image.channels() << std::endl;
    std::cout << "  Depth: " << image.depth() << std::endl;
    std::cout << "  Size: " << image.total() * image.elemSize() << " bytes" << std::endl;
    std::cout << "  Brightness: " << calculateBrightness(image) << std::endl;
    std::cout << "  Contrast: " << calculateContrast(image) << std::endl;
}

cv::Mat CameraUtils::createTestPattern(int width, int height, int pattern_type) {
    cv::Mat pattern(height, width, CV_8UC3);
    
    switch (pattern_type) {
        case 0: { // Color bars
            int bar_width = width / 8;
            std::vector<cv::Scalar> colors = {
                cv::Scalar(255, 255, 255), // White
                cv::Scalar(255, 255, 0),   // Yellow
                cv::Scalar(0, 255, 255),   // Cyan
                cv::Scalar(0, 255, 0),     // Green
                cv::Scalar(255, 0, 255),   // Magenta
                cv::Scalar(255, 0, 0),     // Red
                cv::Scalar(0, 0, 255),     // Blue
                cv::Scalar(0, 0, 0)        // Black
            };
            
            for (int i = 0; i < 8; i++) {
                cv::Rect rect(i * bar_width, 0, bar_width, height);
                cv::rectangle(pattern, rect, colors[i], cv::FILLED);
            }
            break;
        }
        case 1: { // Checkerboard
            int square_size = 50;
            for (int y = 0; y < height; y += square_size) {
                for (int x = 0; x < width; x += square_size) {
                    bool white = ((x / square_size) + (y / square_size)) % 2 == 0;
                    cv::Scalar color = white ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 0);
                    cv::rectangle(pattern, cv::Rect(x, y, square_size, square_size), 
                                color, cv::FILLED);
                }
            }
            break;
        }
        case 2: { // Gradient
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    uchar value = (uchar)((x * 255) / width);
                    pattern.at<cv::Vec3b>(y, x) = cv::Vec3b(value, value, value);
                }
            }
            break;
        }
        default:
            pattern = cv::Scalar(128, 128, 128);
            break;
    }
    
    return pattern;
}

cv::Mat CameraUtils::blendImages(const cv::Mat& img1, const cv::Mat& img2, double alpha) {
    cv::Mat result;
    cv::addWeighted(img1, alpha, img2, 1 - alpha, 0, result);
    return result;
}

cv::Mat CameraUtils::applyFilter(const cv::Mat& image, const std::string& filter_type) {
    cv::Mat result;
    
    if (filter_type == "blur") {
        cv::GaussianBlur(image, result, cv::Size(5, 5), 0);
    } else if (filter_type == "sharpen") {
        cv::Mat kernel = (cv::Mat_<float>(3,3) << 
            0, -1, 0,
            -1, 5, -1,
            0, -1, 0);
        cv::filter2D(image, result, -1, kernel);
    } else if (filter_type == "edge") {
        cv::Canny(image, result, 100, 200);
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    } else if (filter_type == "emboss") {
        cv::Mat kernel = (cv::Mat_<float>(3,3) << 
            -2, -1, 0,
            -1, 1, 1,
            0, 1, 2);
        cv::filter2D(image, result, -1, kernel);
    } else {
        result = image.clone();
    }
    
    return result;
}

cv::Mat CameraUtils::applyColormap(const cv::Mat& image, int colormap_type) {
    cv::Mat result;
    cv::applyColorMap(image, result, colormap_type);
    return result;
}

} // namespace EdgeAI
