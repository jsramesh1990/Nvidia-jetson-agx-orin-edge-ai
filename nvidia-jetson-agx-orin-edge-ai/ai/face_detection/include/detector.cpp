#include "face_detection/detector.h"
#include <NvInfer.h>
#include <cuda_runtime_api.h>
#include <opencv2/dnn.hpp>

namespace EdgeAI {

class FaceDetector::Impl {
public:
    Impl(const std::string& model_path) {
        // Load TensorRT engine
        loadEngine(model_path);
        // Setup CUDA streams
        cudaStreamCreate(&stream);
        // Allocate GPU memory
        allocateBuffers();
    }
    
    ~Impl() {
        cudaStreamDestroy(stream);
        // Free GPU memory
    }
    
    std::vector<FaceBox> detect(const cv::Mat& image) {
        std::vector<FaceBox> faces;
        
        // Preprocess: resize, normalize, convert to RGB
        cv::Mat input;
        preprocess(image, input);
        
        // Copy to GPU
        cudaMemcpyAsync(buffers[0], input.data, 
                        input.total() * input.elemSize(),
                        cudaMemcpyHostToDevice, stream);
        
        // Run inference
        context->enqueueV2(buffers.data(), stream, nullptr);
        
        // Copy results back
        cudaMemcpyAsync(outputBuffer, buffers[1], 
                        outputSize * sizeof(float),
                        cudaMemcpyDeviceToHost, stream);
        cudaStreamSynchronize(stream);
        
        // Post-process
        postprocess(outputBuffer, faces);
        
        return faces;
    }
    
private:
    std::unique_ptr<nvinfer1::ICudaEngine> engine;
    std::unique_ptr<nvinfer1::IExecutionContext> context;
    cudaStream_t stream;
    void* buffers[2];
    float* outputBuffer;
    size_t outputSize;
    
    void loadEngine(const std::string& path) {
        // Load TRT engine from file
        std::ifstream file(path, std::ios::binary);
        std::string engineData((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        
        nvinfer1::IRuntime* runtime = nvinfer1::createInferRuntime(logger);
        engine.reset(runtime->deserializeCudaEngine(engineData.data(),
                                                     engineData.size()));
        context.reset(engine->createExecutionContext());
    }
    
    void allocateBuffers() {
        // Allocate input and output buffers
        cudaMalloc(&buffers[0], inputSize);
        cudaMalloc(&buffers[1], outputSize);
        outputBuffer = new float[outputSize];
    }
    
    void preprocess(const cv::Mat& image, cv::Mat& output) {
        cv::resize(image, output, cv::Size(320, 320));
        output.convertTo(output, CV_32FC3, 1.0/255.0);
        cv::cvtColor(output, output, cv::COLOR_BGR2RGB);
    }
    
    void postprocess(const float* output, std::vector<FaceBox>& faces) {
        // Decode YOLO output
        for (int i = 0; i < numDetections; i++) {
            float confidence = output[4 + i * 6];
            if (confidence > threshold) {
                FaceBox box;
                float x = output[0 + i * 6] * imageWidth;
                float y = output[1 + i * 6] * imageHeight;
                float w = output[2 + i * 6] * imageWidth;
                float h = output[3 + i * 6] * imageHeight;
                box.bbox = cv::Rect(x - w/2, y - h/2, w, h);
                box.confidence = confidence;
                faces.push_back(box);
            }
        }
    }
};

FaceDetector::FaceDetector(const std::string& model_path) 
    : pImpl(std::make_unique<Impl>(model_path)) {}

FaceDetector::~FaceDetector() = default;

std::vector<FaceBox> FaceDetector::detect(const cv::Mat& image) {
    return pImpl->detect(image);
}

} // namespace EdgeAI
