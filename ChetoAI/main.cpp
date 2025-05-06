#include <Windows.h>
#include "overlay.h"
#include "capture.h"
#include "onnx_inference.h"
#include "physics.h"
#include "enums.h"

// Convert YOLO detections to Ball and Table structs
void processDetections(const std::vector<Detection>& detections, Ball& cueBall, Ball& targetBall, Table& table, int screenWidth, int screenHeight) {
    table.pockets.clear();
    bool cueFound = false, targetFound = false;

    for (const auto& det : detections) {
        cv::Point2f center(det.box.x + det.box.width / 2.0f, det.box.y + det.box.height / 2.0f);
        float radius = std::min(det.box.width, det.box.height) / 2.0f;

        // Scale coordinates to overlay resolution
        center.x = (center.x / screenWidth) * 1920.0f;
        center.y = (center.y / screenHeight) * 1080.0f;
        radius = (radius / screenWidth) * 1920.0f;

        switch (static_cast<ObjectType>(det.class_id)) {
        case ObjectType::White:
            cueBall = { center, radius, BallType::Cue }; // Type 0 = cue
            cueFound = true;
            break;
        case ObjectType::Ball:
            if (!targetFound) { // Pick the first detected ball as target
                targetBall = { center, radius, BallType::Target }; // Type 1 = target
                targetFound = true;
            }
            break;
        case ObjectType::Hole:
            table.pockets.push_back(center);
            break;
        case ObjectType::PlayArea:
            table.bounds = cv::Rect(
                (det.box.x / screenWidth) * 1920.0f,
                (det.box.y / screenHeight) * 1080.0f,
                (det.box.width / screenWidth) * 1920.0f,
                (det.box.height / screenHeight) * 1080.0f
            );
            break;
        default:
            break;
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize overlay
    OverlayData overlayData;
    HWND overlayHwnd = InitializeOverlay(hInstance, &overlayData);
    if (!overlayHwnd) {
        MessageBoxA(nullptr, "Failed to initialize overlay!", "Error", MB_OK);
        return 1;
    }

    // Initialize ONNX inference
    ONNXInference detector("onnx_model/yolov11m-seg.onnx");

    // Check if model loaded successfully
    if (!detector.isSessionValid()) {
        MessageBoxA(nullptr, "Failed to load ONNX model!", "Error", MB_OK);
        return 1;
    }

    // Initialize screen capture
    while (true) {
        cv::Mat frame = captureWindow(L"8 Ball Pool");
        if (frame.empty()) continue;

        int frameWidth = frame.cols;
        int frameHeight = frame.rows;

        std::vector<Detection> detections = detector.runInference(frame);

        Ball cueBall, targetBall;
        Table table;
        processDetections(detections, cueBall, targetBall, table, frameWidth, frameHeight);

        ClearOverlay(&overlayData);

        std::vector<LineSegment> guide = calculateGuideline(cueBall, targetBall, table);
        float red[4] = { 1.0f, 0, 0, 1.0f };
        for (const auto& segment : guide)
            DrawLine(segment.start.x, segment.start.y, segment.end.x, segment.end.y, red, &overlayData);

        PresentOverlay(&overlayData);

        if (GetAsyncKeyState(VK_END) & 1) break;
        Sleep(16);
    }

    CleanupOverlay(&overlayData);
    return 0;
}
