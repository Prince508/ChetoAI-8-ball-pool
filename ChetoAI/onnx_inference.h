#pragma once

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <C:/onnxruntime/include/cpu_provider_factory.h>
#include <C:/onnxruntime/include/onnxruntime_cxx_api.h>
#include <C:/onnxruntime/include/onnxruntime_c_api.h>
#include "enums.h"
#include "physics.h"
#include <memory>
#include <stdexcept>
#include <locale>
#include <codecvt>

struct Detection {
    cv::Rect box;
    int class_id;
    float confidence;
    ObjectType type = ObjectType::Unknown;
    BallType ball_type = BallType::Other;
};

class ONNXInference {
public:
    ONNXInference(const std::string& modelPath);
    std::vector<Detection> runInference(const cv::Mat& frame);
    bool isSessionValid() const { return valid; }

private:
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    Ort::SessionOptions sessionOptions;
    std::vector<const char*> inputNames;
    std::vector<const char*> outputNames;
    bool valid = false;
    std::vector<std::string> inputNamesStr; // Stores input names as strings
    std::vector<std::string> outputNamesStr; // Stores output names as strings
    const int inputWidth = 640;
    const int inputHeight = 640;
};