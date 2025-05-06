#include "onnx_inference.h"
#include <Windows.h>
#include <algorithm>
#include "Enums.h"
#include <string>

#ifndef ORTCHAR_T
#ifdef _WIN32
#define ORTCHAR_T wchar_t
#else
#define ORTCHAR_T char
#endif
#endif

// Converts UTF-8 std::string to std::wstring (common requirement on Windows)
std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size_needed <= 0) return std::wstring();

    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    wstr.pop_back(); // Remove the null terminator added by MultiByteToWideChar
    return wstr;
}

// Converts std::wstring to UTF-8 std::string
std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return std::string();

    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);
    str.pop_back(); // Remove the null terminator added by WideCharToMultiByte
    return str;
}


ONNXInference::ONNXInference(const std::string& modelPath)
    : env(ORT_LOGGING_LEVEL_WARNING, "ChetoAI") {
    try {
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        // Convert the std::string model path to the required wide string format for Windows
        #ifdef _WIN32
            std::wstring modelPathW = utf8_to_wstring(modelPath);
            const ORTCHAR_T* modelPathPtr = modelPathW.c_str();
        #else
            // On non-Windows platforms, ONNX Runtime usually expects char*
            const ORTCHAR_T* modelPathPtr = modelPath.c_str();
        #endif

        // Now create the session using the correctly typed path pointer
        session = std::make_unique<Ort::Session>(env, modelPathPtr, sessionOptions);
        valid = true;

        Ort::AllocatorWithDefaultOptions allocator;

        // Use the C API to get the input name
        char* inputName = nullptr;
        Ort::GetApi().SessionGetInputName(*session, 0, allocator, &inputName);
        inputNamesStr.emplace_back(std::string(inputName));
        allocator.Free(inputName);

        // Use the C API to get the output name
        char* outputName = nullptr;
        Ort::GetApi().SessionGetOutputName(*session, 0, allocator, &outputName);
        outputNamesStr.emplace_back(std::string(outputName));
        allocator.Free(outputName);
    }
    catch (const Ort::Exception& e) {
        valid = false;
        MessageBoxA(nullptr, e.what(), "ONNX Load Error", MB_OK | MB_ICONERROR);
    }
}

std::vector<Detection> ONNXInference::runInference(const cv::Mat& frame) {
    std::vector<Detection> detections;
    if (!valid) return detections;

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(inputWidth, inputHeight));
    cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
    resized.convertTo(resized, CV_32F, 1.0 / 255.0);

    std::vector<float> inputTensorValues(inputWidth * inputHeight * 3);
    for (int c = 0; c < 3; ++c)
        for (int h = 0; h < inputHeight; ++h)
            for (int w = 0; w < inputWidth; ++w)
                inputTensorValues[c * inputWidth * inputHeight + h * inputWidth + w] =
                resized.at<cv::Vec3f>(h, w)[c];

    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeDefault);
    std::vector<int64_t> inputShape = { 1, 3, inputHeight, inputWidth };

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, inputTensorValues.data(), inputTensorValues.size(), inputShape.data(), inputShape.size());

    std::vector<Ort::Value> outputTensors = session->Run(
        Ort::RunOptions{ nullptr }, inputNames.data(), &inputTensor, 1, outputNames.data(), 1);

    float* outputData = outputTensors[0].GetTensorMutableData<float>();
    size_t outputSize = outputTensors[0].GetTensorTypeAndShapeInfo().GetElementCount();

    for (size_t i = 0; i < outputSize; i += 6) {
        float conf = outputData[i + 4];
        if (conf > 0.4f) {
            Detection det;
            det.box = cv::Rect(
                static_cast<int>(outputData[i]),
                static_cast<int>(outputData[i + 1]),
                static_cast<int>(outputData[i + 2]),
                static_cast<int>(outputData[i + 3]));
            det.confidence = conf;
            det.class_id = static_cast<int>(outputData[i + 5]);
            detections.push_back(det);
        }
    }

    return detections;
}