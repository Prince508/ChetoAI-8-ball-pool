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

        size_t outputs = session->GetOutputCount();
        sprintf_s(buf, "[Model] Number of outputs: %zu\n", outputs);
        OutputDebugStringA(buf);

        for (size_t i = 0; i < outputs; ++i) {
            Ort::TypeInfo outInfo = session->GetOutputTypeInfo(i);
            auto shape = outInfo.GetTensorTypeAndShapeInfo().GetShape();
            sprintf_s(buf, "Output %zu shape: ", i);
            OutputDebugStringA(buf);
            for (auto dim : shape) {
                sprintf_s(buf, "%lld ", dim);
                OutputDebugStringA(buf);
            }
            OutputDebugStringA("\n");
        };

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

    // Preprocess image
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
        memoryInfo, inputTensorValues.data(), inputTensorValues.size(),
        inputShape.data(), inputShape.size());

    std::vector<Ort::Value> outputTensors;
    try {
        outputTensors = session->Run(
            Ort::RunOptions{ nullptr }, inputNames.data(), &inputTensor, 1,
            outputNames.data(), outputNames.size());
    }
    catch (const Ort::Exception& e) {
        std::cerr << "[ONNX Runtime ERROR] " << e.what() << std::endl;
        return detections;
    }

    // === Output 0: Bounding Boxes ===
    float* output = outputTensors[0].GetTensorMutableData<float>();
    auto shape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
    const int numChannels = (int)shape[1];
    const int numBoxes = (int)shape[2];
    const int numClasses = numChannels - 4;

    char buf[256];
    for (int i = 0; i < numBoxes; ++i) {
        float x = output[0 * numBoxes + i];
        float y = output[1 * numBoxes + i];
        float w = output[2 * numBoxes + i];
        float h = output[3 * numBoxes + i];

        int bestClass = -1;
        float bestScore = 0.0f;
        for (int c = 4; c < numChannels; ++c) {
            float score = output[c * numBoxes + i];
            if (score > bestScore) {
                bestScore = score;
                bestClass = c - 4;
            }
        }

        if (bestScore > 0.5f) {
            Detection det;
            det.box = cv::Rect(
                static_cast<int>(x - w / 2.0f),
                static_cast<int>(y - h / 2.0f),
                static_cast<int>(w),
                static_cast<int>(h));
            det.confidence = bestScore;
            det.class_id = bestClass;
            detections.push_back(det);

            sprintf_s(buf, "[Box] Class %d | Conf %.2f | x=%.1f y=%.1f w=%.1f h=%.1f\n",
                bestClass, bestScore, x, y, w, h);
            OutputDebugStringA(buf);
        }
    }

    sprintf_s(buf, "[Detection] Total boxes: %zu\n", detections.size());
    OutputDebugStringA(buf);

    // === Output 1: Segmentation Map [1, 32, 160, 160] ===
    float* segData = outputTensors[1].GetTensorMutableData<float>();
    auto segShape = outputTensors[1].GetTensorTypeAndShapeInfo().GetShape(); // [1, 32, 160, 160]
    int segChannels = (int)segShape[1]; // 32
    int segH = (int)segShape[2];
    int segW = (int)segShape[3];

    // Debug dump guideline mask (class 6)
    int guidelineClass = 6; // 'Guideline'
    cv::Mat mask(segH, segW, CV_32FC1);
    for (int y = 0; y < segH; ++y) {
        for (int x = 0; x < segW; ++x) {
            int idx = guidelineClass * segH * segW + y * segW + x;
            mask.at<float>(y, x) = segData[idx];
        }
    }

    // Apply threshold and convert to visual format
    cv::Mat binaryMask;
    cv::threshold(mask, binaryMask, 0.5, 1.0, cv::THRESH_BINARY);
    binaryMask.convertTo(binaryMask, CV_8U, 255);

    // Optional: save for debug
    cv::imwrite("guideline_mask.jpg", binaryMask);

    OutputDebugStringA("[Segmentation] Guideline mask saved.\n");

    return detections;
}


