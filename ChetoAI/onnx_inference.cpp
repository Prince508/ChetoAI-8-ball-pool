#include "onnx_inference.h"
#include <Windows.h>
#include <iostream>
#include <string>
#include <algorithm>
#include "enums.h"

ONNXInference::ONNXInference(const std::string& modelPath)
    : env(ORT_LOGGING_LEVEL_WARNING, "ChetoAI") {
    try {
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        // Load model
        std::wstring modelPathW(modelPath.begin(), modelPath.end());
        session = std::make_unique<Ort::Session>(env, modelPathW.c_str(), sessionOptions);
        valid = true;

        // Get input/output names
        Ort::AllocatorWithDefaultOptions allocator;
        Ort::AllocatedStringPtr inputName = session->GetInputNameAllocated(0, allocator);
        Ort::AllocatedStringPtr outputName = session->GetOutputNameAllocated(0, allocator);
        inputNames.push_back(inputName.get());
        outputNames.push_back(outputName.get());

        // Output model shape for debugging
        Ort::TypeInfo inputTypeInfo = session->GetInputTypeInfo(0);
        auto inputShape = inputTypeInfo.GetTensorTypeAndShapeInfo().GetShape();
        Ort::TypeInfo outputTypeInfo = session->GetOutputTypeInfo(0);
        auto outputShape = outputTypeInfo.GetTensorTypeAndShapeInfo().GetShape();

        char buf[256];
        sprintf_s(buf, "[Model] Input shape: %lldx%lldx%lldx%lld\n",
            inputShape[0], inputShape[1], inputShape[2], inputShape[3]);
        OutputDebugStringA(buf);

        sprintf_s(buf, "[Model] Output shape total elements: %lld\n",
            outputTypeInfo.GetTensorTypeAndShapeInfo().GetElementCount());
        OutputDebugStringA(buf);

    }
    catch (const Ort::Exception& e) {
        valid = false;
        MessageBoxA(nullptr, e.what(), "ONNX Load Error", MB_OK | MB_ICONERROR);
    }
}

std::vector<Detection> ONNXInference::runInference(const cv::Mat& frame) {
    std::vector<Detection> detections;
    
    if (!valid) {
        std::cerr << "[ERROR] ONNX model session is not valid." << std::endl;
        return detections;
    }

    if (frame.empty()) {
        std::cerr << "[ERROR] Input frame is empty." << std::endl;
        return detections;
    }

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

    std::vector<Ort::Value> outputTensors;
    try {
        outputTensors = session->Run(
            Ort::RunOptions{ nullptr }, inputNames.data(), &inputTensor, 1, outputNames.data(), 1);
    }
    catch (const Ort::Exception& e) {
        std::cerr << "[ONNX Runtime ERROR] " << e.what() << std::endl;
        return detections;
    }

    char buf[256];
    float* outputData = outputTensors[0].GetTensorMutableData<float>();
    size_t outputSize = outputTensors[0].GetTensorTypeAndShapeInfo().GetElementCount();

    sprintf_s(buf, "Raw output size: %zu\n", outputSize);
    OutputDebugStringA(buf);

    for (size_t i = 0; i + 5 < outputSize && i < 24; i += 6) {
        sprintf_s(buf, "Det raw: [x=%.1f y=%.1f w=%.1f h=%.1f conf=%.2f class=%.0f]\n",
            outputData[i], outputData[i + 1], outputData[i + 2],
            outputData[i + 3], outputData[i + 4], outputData[i + 5]);
        OutputDebugStringA(buf);
    }


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
    std::cout << "Detected: " << detections.size() << " objects." << std::endl;

    return detections;
}
